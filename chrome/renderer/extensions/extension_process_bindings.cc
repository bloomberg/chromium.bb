// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_process_bindings.h"

#include "base/singleton.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/extensions/bindings_utils.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/extensions/renderer_extension_bindings.h"
#include "chrome/renderer/js_only_v8_extensions.h"
#include "chrome/renderer/render_view.h"
#include "grit/common_resources.h"
#include "grit/renderer_resources.h"
#include "webkit/api/public/WebFrame.h"

using bindings_utils::GetStringResource;
using bindings_utils::ContextInfo;
using bindings_utils::ContextList;
using bindings_utils::GetContexts;
using bindings_utils::GetPendingRequestMap;
using bindings_utils::PendingRequest;
using bindings_utils::PendingRequestMap;
using bindings_utils::ExtensionBase;

namespace {

// A map of extension ID to vector of page action ids.
typedef std::map< std::string, std::vector<std::string> > PageActionIdMap;

// A map of permission name to whether its enabled for this extension.
typedef std::map<std::string, bool> PermissionsMap;

// A map of extension ID to permissions map.
typedef std::map<std::string, PermissionsMap> ExtensionPermissionsMap;

const char kExtensionName[] = "chrome/ExtensionProcessBindings";
const char* kExtensionDeps[] = {
  BaseJsV8Extension::kName,
  EventBindings::kName,
  JsonSchemaJsV8Extension::kName,
  RendererExtensionBindings::kName,
};

struct SingletonData {
  std::set<std::string> function_names_;
  PageActionIdMap page_action_ids_;
  ExtensionPermissionsMap permissions_;
};

static std::set<std::string>* GetFunctionNameSet() {
  return &Singleton<SingletonData>()->function_names_;
}

static PageActionIdMap* GetPageActionMap() {
  return &Singleton<SingletonData>()->page_action_ids_;
}

static PermissionsMap* GetPermissionsMap(const std::string& extension_id) {
  return &Singleton<SingletonData>()->permissions_[extension_id];
}

class ExtensionImpl : public ExtensionBase {
 public:
  ExtensionImpl() : ExtensionBase(
      kExtensionName, GetStringResource<IDR_EXTENSION_PROCESS_BINDINGS_JS>(),
      arraysize(kExtensionDeps), kExtensionDeps) {}

  static void SetFunctionNames(const std::vector<std::string>& names) {
    std::set<std::string>* name_set = GetFunctionNameSet();
    for (size_t i = 0; i < names.size(); ++i) {
      name_set->insert(names[i]);
    }
  }

  // Note: do not call this function before or during the chromeHidden.onLoad
  // event dispatch. The URL might not have been committed yet and might not
  // be an extension URL.
  static std::string ExtensionIdForCurrentContext() {
    RenderView* renderview = bindings_utils::GetRenderViewForCurrentContext();
    DCHECK(renderview);
    GURL url = renderview->webview()->GetMainFrame()->url();
    if (url.SchemeIs(chrome::kExtensionScheme))
      return url.host();
    return std::string();
  }

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) {
    if (name->Equals(v8::String::New("GetExtensionAPIDefinition"))) {
      return v8::FunctionTemplate::New(GetExtensionAPIDefinition);
    } else if (name->Equals(v8::String::New("GetExtensionViews"))) {
      return v8::FunctionTemplate::New(GetExtensionViews);
    } else if (name->Equals(v8::String::New("GetNextRequestId"))) {
      return v8::FunctionTemplate::New(GetNextRequestId);
    } else if (name->Equals(v8::String::New("OpenChannelToTab"))) {
      return v8::FunctionTemplate::New(OpenChannelToTab);
    } else if (name->Equals(v8::String::New("GetCurrentPageActions"))) {
      return v8::FunctionTemplate::New(GetCurrentPageActions);
    } else if (name->Equals(v8::String::New("StartRequest"))) {
      return v8::FunctionTemplate::New(StartRequest);
    }

    return ExtensionBase::GetNativeFunction(name);
  }

 private:
  static v8::Handle<v8::Value> GetExtensionAPIDefinition(
      const v8::Arguments& args) {
    return v8::String::New(GetStringResource<IDR_EXTENSION_API_JSON>());
  }

  static v8::Handle<v8::Value> GetExtensionViews(const v8::Arguments& args) {
    if (args.Length() != 2)
      return v8::Undefined();

    if (!args[0]->IsInt32() || !args[1]->IsString())
      return v8::Undefined();

    // |browser_window_id| == -1 means getting views attached to any browser
    // window.
    int browser_window_id = args[0]->Int32Value();

    std::string view_type_string = *v8::String::Utf8Value(args[1]->ToString());
    // |view_type| == ViewType::INVALID means getting any type of views.
    ViewType::Type view_type = ViewType::INVALID;
    if (view_type_string == "TOOLSTRIP") {
      view_type = ViewType::EXTENSION_TOOLSTRIP;
    } else if (view_type_string == "BACKGROUND") {
      view_type = ViewType::EXTENSION_BACKGROUND_PAGE;
    } else if (view_type_string == "TAB") {
      view_type = ViewType::TAB_CONTENTS;
    } else if (view_type_string != "ALL") {
      return v8::Undefined();
    }

    v8::Local<v8::Array> views = v8::Array::New();
    int index = 0;
    RenderView::RenderViewSet* render_view_set_pointer =
        Singleton<RenderView::RenderViewSet>::get();
    DCHECK(render_view_set_pointer->render_view_set_.size() > 0);

    v8::Local<v8::Value> window;
    std::string current_extension_id = ExtensionIdForCurrentContext();
    std::set<RenderView* >::iterator it =
        render_view_set_pointer->render_view_set_.begin();
    for (; it != render_view_set_pointer->render_view_set_.end(); ++it) {
      if (view_type != ViewType::INVALID && (*it)->view_type() != view_type)
        continue;

      GURL url = (*it)->webview()->GetMainFrame()->url();
      if (!url.SchemeIs(chrome::kExtensionScheme))
        continue;
      std::string extension_id = url.host();
      if (extension_id != current_extension_id)
        continue;

      if (browser_window_id != -1 &&
          (*it)->browser_window_id() != browser_window_id) {
        continue;
      }

      v8::Local<v8::Context> context =
          (*it)->webview()->GetMainFrame()->mainWorldScriptContext();
      if (!context.IsEmpty()) {
        v8::Local<v8::Value> window = context->Global();
        DCHECK(!window.IsEmpty());
        views->Set(v8::Integer::New(index), window);
        index++;
        if (view_type == ViewType::EXTENSION_BACKGROUND_PAGE)
          break;
      }
    }
    return views;
  }

  static v8::Handle<v8::Value> GetNextRequestId(const v8::Arguments& args) {
    static int next_request_id = 0;
    return v8::Integer::New(next_request_id++);
  }

  // Creates a new messaging channel to the tab with the given ID.
  static v8::Handle<v8::Value> OpenChannelToTab(const v8::Arguments& args) {
    // Get the current RenderView so that we can send a routed IPC message from
    // the correct source.
    RenderView* renderview = bindings_utils::GetRenderViewForCurrentContext();
    if (!renderview)
      return v8::Undefined();

    if (args.Length() >= 3 && args[0]->IsInt32() && args[1]->IsString() &&
        args[2]->IsString()) {
      int tab_id = args[0]->Int32Value();
      std::string extension_id = *v8::String::Utf8Value(args[1]->ToString());
      std::string channel_name = *v8::String::Utf8Value(args[2]->ToString());
      int port_id = -1;
      renderview->Send(new ViewHostMsg_OpenChannelToTab(
          renderview->routing_id(), tab_id, extension_id, channel_name,
          &port_id));
      return v8::Integer::New(port_id);
    }
    return v8::Undefined();
  }

  static v8::Handle<v8::Value> GetCurrentPageActions(
      const v8::Arguments& args) {
    std::string extension_id = *v8::String::Utf8Value(args[0]->ToString());
    PageActionIdMap* page_action_map = GetPageActionMap();
    PageActionIdMap::const_iterator it = page_action_map->find(extension_id);

    std::vector<std::string> page_actions;
    size_t size  = 0;
    if (it != page_action_map->end()) {
      page_actions = it->second;
      size = page_actions.size();
    }

    v8::Local<v8::Array> page_action_vector = v8::Array::New(size);
    for (size_t i = 0; i < size; ++i) {
      std::string page_action_id = page_actions[i];
      page_action_vector->Set(v8::Integer::New(i),
                              v8::String::New(page_action_id.c_str()));
    }

    return page_action_vector;
  }

  // Starts an API request to the browser, with an optional callback.  The
  // callback will be dispatched to EventBindings::HandleResponse.
  static v8::Handle<v8::Value> StartRequest(const v8::Arguments& args) {
    // Get the current RenderView so that we can send a routed IPC message from
    // the correct source.
    RenderView* renderview = bindings_utils::GetRenderViewForCurrentContext();
    if (!renderview)
      return v8::Undefined();

    if (args.Length() != 4 || !args[0]->IsString() || !args[1]->IsString() ||
        !args[2]->IsInt32() || !args[3]->IsBoolean())
      return v8::Undefined();

    std::string name = *v8::String::AsciiValue(args[0]);
    if (GetFunctionNameSet()->find(name) == GetFunctionNameSet()->end()) {
      NOTREACHED() << "Unexpected function " << name;
      return v8::Undefined();
    }

    if (!ExtensionProcessBindings::CurrentContextHasPermission(name)) {
#if EXTENSION_TIME_TO_BREAK_API
      return ExtensionProcessBindings::ThrowPermissionDeniedException(name);
#else
      ExtensionProcessBindings::ThrowPermissionDeniedException(name);
#endif
    }

    std::string json_args = *v8::String::Utf8Value(args[1]);
    int request_id = args[2]->Int32Value();
    bool has_callback = args[3]->BooleanValue();

    v8::Persistent<v8::Context> current_context =
        v8::Persistent<v8::Context>::New(v8::Context::GetCurrent());
    DCHECK(!current_context.IsEmpty());
    GetPendingRequestMap()[request_id].reset(new PendingRequest(
        current_context, name));

    renderview->SendExtensionRequest(name, json_args, request_id, has_callback);

    return v8::Undefined();
  }
};

}  // namespace

v8::Extension* ExtensionProcessBindings::Get() {
  static v8::Extension* extension = new ExtensionImpl();
  return extension;
}

void ExtensionProcessBindings::SetFunctionNames(
    const std::vector<std::string>& names) {
  ExtensionImpl::SetFunctionNames(names);
}

// static
void ExtensionProcessBindings::HandleResponse(int request_id, bool success,
                                              const std::string& response,
                                              const std::string& error) {
  PendingRequestMap& pending_requests = GetPendingRequestMap();
  PendingRequestMap::iterator request = pending_requests.find(request_id);
  if (request == pending_requests.end())
    return;  // The frame went away.

  v8::HandleScope handle_scope;
  v8::Handle<v8::Value> argv[5];
  argv[0] = v8::Integer::New(request_id);
  argv[1] = v8::String::New(request->second->name.c_str());
  argv[2] = v8::Boolean::New(success);
  argv[3] = v8::String::New(response.c_str());
  argv[4] = v8::String::New(error.c_str());
  v8::Handle<v8::Value> retval = bindings_utils::CallFunctionInContext(
      request->second->context, "handleResponse", arraysize(argv), argv);
  // In debug, the js will validate the callback parameters and return a 
  // string if a validation error has occured.
#ifdef _DEBUG
  if (!retval.IsEmpty() && !retval->IsUndefined()) {
    std::string error = *v8::String::AsciiValue(retval);
    DCHECK(false) << error;
  }
#endif

  pending_requests.erase(request);
}

// static
void ExtensionProcessBindings::SetPageActions(
    const std::string& extension_id,
    const std::vector<std::string>& page_actions) {
  PageActionIdMap& page_action_map = *GetPageActionMap();
  if (!page_actions.empty()) {
    page_action_map[extension_id] = page_actions;
  } else {
    if (page_action_map.find(extension_id) != page_action_map.end())
      page_action_map.erase(extension_id);
  }
}

// static
void ExtensionProcessBindings::SetPermissions(
    const std::string& extension_id,
    const std::vector<std::string>& permissions) {
  PermissionsMap& permissions_map = *GetPermissionsMap(extension_id);

  // Default all permissions to false, then enable the ones in the vector.
  for (size_t i = 0; i < Extension::kNumPermissions; ++i)
    permissions_map[Extension::kPermissionNames[i]] = false;
  for (size_t i = 0; i < permissions.size(); ++i)
    permissions_map[permissions[i]] = true;
}

// Given a name like "tabs.onConnect", return the permission name required
// to access that API ("tabs" in this example).
static std::string GetPermissionName(const std::string& function_name) {
  size_t first_dot = function_name.find('.');
  std::string permission_name = function_name.substr(0, first_dot);
  if (permission_name == "windows")
    return "tabs";  // windows and tabs are the same permission.
  return permission_name;
}

// static
bool ExtensionProcessBindings::CurrentContextHasPermission(
    const std::string& function_name) {
  std::string extension_id = ExtensionImpl::ExtensionIdForCurrentContext();
  PermissionsMap& permissions_map = *GetPermissionsMap(extension_id);
  std::string permission_name = GetPermissionName(function_name);
  PermissionsMap::iterator it = permissions_map.find(permission_name);

  // We explicitly check if the permission entry is present and false, because
  // some APIs do not have a required permission entry (ie, "chrome.self").
  return (it == permissions_map.end() || it->second);
}

// static
v8::Handle<v8::Value>
    ExtensionProcessBindings::ThrowPermissionDeniedException(
      const std::string& function_name) {
  static const char kMessage[] =
      "You do not have permission to use 'chrome.%s'. Be sure to declare"
      " in your manifest what permissions you need.";
  std::string permission_name = GetPermissionName(function_name);
  std::string error_msg = StringPrintf(kMessage, permission_name.c_str());

#if EXTENSION_TIME_TO_BREAK_API
  return v8::ThrowException(v8::Exception::Error(
      v8::String::New(error_msg.c_str())));
#else
  // Call console.error for now.

  v8::HandleScope scope;

  v8::Local<v8::Value> console =
      v8::Context::GetCurrent()->Global()->Get(v8::String::New("console"));
  v8::Local<v8::Value> console_error;
  if (!console.IsEmpty() && console->IsObject())
    console_error = console->ToObject()->Get(v8::String::New("error"));
  if (console_error.IsEmpty() || !console_error->IsFunction())
    return v8::Undefined();

  v8::Local<v8::Function> function =
      v8::Local<v8::Function>::Cast(console_error);
  v8::Local<v8::Value> argv[] = { v8::String::New(error_msg.c_str()) };
  if (!function.IsEmpty())
    function->Call(console->ToObject(), arraysize(argv), argv);

  return v8::Undefined();
#endif
}
