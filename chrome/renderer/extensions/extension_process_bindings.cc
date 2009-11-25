// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_process_bindings.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/singleton.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_message_bundle.h"
#include "chrome/common/extensions/url_pattern.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/extensions/bindings_utils.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/extensions/js_only_v8_extensions.h"
#include "chrome/renderer/extensions/renderer_extension_bindings.h"
#include "chrome/renderer/render_view.h"
#include "grit/common_resources.h"
#include "grit/renderer_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSecurityPolicy.h"

using bindings_utils::GetStringResource;
using bindings_utils::ContextInfo;
using bindings_utils::ContextList;
using bindings_utils::GetContexts;
using bindings_utils::GetPendingRequestMap;
using bindings_utils::PendingRequest;
using bindings_utils::PendingRequestMap;
using bindings_utils::ExtensionBase;
using WebKit::WebFrame;
using WebKit::WebSecurityPolicy;
using WebKit::WebView;

namespace {

// A map of extension ID to vector of page action ids.
typedef std::map< std::string, std::vector<std::string> > PageActionIdMap;

// A map of permission name to whether its enabled for this extension.
typedef std::map<std::string, bool> PermissionsMap;

// A map of extension ID to permissions map.
typedef std::map<std::string, PermissionsMap> ExtensionPermissionsMap;

// A map of message name to message.
typedef std::map<std::string, std::string> L10nMessagesMap;

// A map of extension ID to l10n message map.
typedef std::map<std::string, L10nMessagesMap >
  ExtensionToL10nMessagesMap;

const char kExtensionName[] = "chrome/ExtensionProcessBindings";
const char* kExtensionDeps[] = {
  BaseJsV8Extension::kName,
  EventBindings::kName,
  JsonSchemaJsV8Extension::kName,
  RendererExtensionBindings::kName,
  ExtensionApiTestV8Extension::kName,
};

struct SingletonData {
  std::set<std::string> function_names_;
  PageActionIdMap page_action_ids_;
  ExtensionPermissionsMap permissions_;
  ExtensionToL10nMessagesMap extension_l10n_messages_map_;
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

static ExtensionToL10nMessagesMap* GetExtensionToL10nMessagesMap() {
  return &Singleton<SingletonData>()->extension_l10n_messages_map_;
}

static L10nMessagesMap* GetL10nMessagesMap(const std::string extension_id) {
  ExtensionToL10nMessagesMap::iterator it =
    Singleton<SingletonData>()->extension_l10n_messages_map_.find(extension_id);
  if (it != Singleton<SingletonData>()->extension_l10n_messages_map_.end()) {
    return &(it->second);
  } else {
    return NULL;
  }
}

// A RenderViewVisitor class that iterates through the set of available
// views, looking for a view of the given type, in the given browser window
// and within the given extension.
// Used to accumulate the list of views associated with an extension.
class ExtensionViewAccumulator : public RenderViewVisitor {
 public:
  ExtensionViewAccumulator(const std::string& extension_id,
                           int browser_window_id,
                           ViewType::Type view_type)
      : extension_id_(extension_id),
        browser_window_id_(browser_window_id),
        view_type_(view_type),
        views_(v8::Array::New()),
        index_(0) {
  }

  v8::Local<v8::Array> views() { return views_; }

  virtual bool Visit(RenderView* render_view) {
    if (!ViewTypeMatches(render_view->view_type(), view_type_))
      return true;

    GURL url = render_view->webview()->mainFrame()->url();
    if (!url.SchemeIs(chrome::kExtensionScheme))
      return true;
    const std::string& extension_id = url.host();
    if (extension_id != extension_id_)
      return true;

    if (browser_window_id_ != -1 &&
        render_view->browser_window_id() != browser_window_id_)
      return true;

    v8::Local<v8::Context> context =
        render_view->webview()->mainFrame()->mainWorldScriptContext();
    if (!context.IsEmpty()) {
      v8::Local<v8::Value> window = context->Global();
      DCHECK(!window.IsEmpty());

      if (!OnMatchedView(window))
        return false;
    }
    return true;
  }

 private:
  // Called on each view found matching the search criteria.  Returns false
  // to terminate the iteration.
  bool OnMatchedView(const v8::Local<v8::Value>& view_window) {
    views_->Set(v8::Integer::New(index_), view_window);
    index_++;

    if (view_type_ == ViewType::EXTENSION_BACKGROUND_PAGE)
      return false;  // There can be only one...

    return true;
  }

  // Returns true is |type| "isa" |match|.
  static bool ViewTypeMatches(ViewType::Type type, ViewType::Type match) {
    if (type == match)
      return true;

    // INVALID means match all.
    if (match == ViewType::INVALID)
      return true;

    // TODO(erikkay) for now, special case mole as a type of toolstrip.
    // Perhaps this isn't the right long-term thing to do.
    if (match == ViewType::EXTENSION_TOOLSTRIP &&
        type == ViewType::EXTENSION_MOLE) {
      return true;
    }

    return false;
  }

  std::string extension_id_;
  int browser_window_id_;
  ViewType::Type view_type_;
  v8::Local<v8::Array> views_;
  int index_;
};

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
    if (!renderview)
      return std::string();  // this can happen as a tab is closing.

    GURL url = renderview->webview()->mainFrame()->url();
    if (!url.SchemeIs(chrome::kExtensionScheme))
      return std::string();

    return url.host();
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
    } else if (name->Equals(v8::String::New("GetRenderViewId"))) {
      return v8::FunctionTemplate::New(GetRenderViewId);
    } else if (name->Equals(v8::String::New("GetL10nMessage"))) {
      return v8::FunctionTemplate::New(GetL10nMessage);
    } else if (name->Equals(v8::String::New("GetPopupView"))) {
      return v8::FunctionTemplate::New(GetPopupView);
    } else if (name->Equals(v8::String::New("GetPopupParentWindow"))) {
      return v8::FunctionTemplate::New(GetPopupParentWindow);
    } else if (name->Equals(v8::String::New("SetExtensionActionIcon"))) {
      return v8::FunctionTemplate::New(SetExtensionActionIcon);
    }

    return ExtensionBase::GetNativeFunction(name);
  }

 private:
  static v8::Handle<v8::Value> GetExtensionAPIDefinition(
      const v8::Arguments& args) {
    return v8::String::New(GetStringResource<IDR_EXTENSION_API_JSON>());
  }

  static v8::Handle<v8::Value> PopupViewFinder(
      const v8::Arguments& args,
      ViewType::Type viewtype_to_find) {
    // TODO(twiz)  Correct the logic that ties the ownership of the pop-up view
    // to the hosting view.  At the moment we assume that there may only be
    // a single pop-up view for a given extension.  By doing so, we can find
    // the pop-up view by simply searching for the only pop-up view present.
    // We also assume that if the current view is a pop-up, we can find the
    // hosting view by searching for a TOOLSTRIP view.
    if (args.Length() != 0)
      return v8::Undefined();

    if (viewtype_to_find != ViewType::EXTENSION_POPUP &&
        viewtype_to_find != ViewType::EXTENSION_TOOLSTRIP) {
      NOTREACHED() << L"Requesting invalid view type.";
    }

    // Disallow searching for a host view if we are a popup view, and likewise
    // if we are a toolstrip view.
    RenderView* render_view = bindings_utils::GetRenderViewForCurrentContext();
    if (!render_view ||
        render_view->view_type() == viewtype_to_find) {
      return v8::Undefined();
    }

    int browser_window_id = render_view->browser_window_id();
    std::string extension_id = ExtensionIdForCurrentContext();
    if (extension_id.empty())
      return v8::Undefined();

    ExtensionViewAccumulator  popup_matcher(extension_id,
                                            browser_window_id,
                                            viewtype_to_find);
    RenderView::ForEach(&popup_matcher);

    if (0 == popup_matcher.views()->Length())
      return v8::Undefined();
    DCHECK(popup_matcher.views()->Has(0));

    // Return the first view found.
    return popup_matcher.views()->Get(v8::Integer::New(0));
  }

  static v8::Handle<v8::Value> GetPopupView(const v8::Arguments& args) {
    return PopupViewFinder(args, ViewType::EXTENSION_POPUP);
  }

  static v8::Handle<v8::Value> GetPopupParentWindow(const v8::Arguments& args) {
    return PopupViewFinder(args, ViewType::EXTENSION_TOOLSTRIP);
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
    if (view_type_string == ViewType::kToolstrip) {
      view_type = ViewType::EXTENSION_TOOLSTRIP;
    } else if (view_type_string == ViewType::kMole) {
      view_type = ViewType::EXTENSION_MOLE;
    } else if (view_type_string == ViewType::kBackgroundPage) {
      view_type = ViewType::EXTENSION_BACKGROUND_PAGE;
    } else if (view_type_string == ViewType::kTabContents) {
      view_type = ViewType::TAB_CONTENTS;
    } else if (view_type_string == ViewType::kPopup) {
      view_type = ViewType::EXTENSION_POPUP;
    } else if (view_type_string != ViewType::kAll) {
      return v8::Undefined();
    }

    std::string extension_id = ExtensionIdForCurrentContext();
    if (extension_id.empty())
      return v8::Undefined();

    ExtensionViewAccumulator accumulator(extension_id, browser_window_id,
                                         view_type);
    RenderView::ForEach(&accumulator);
    return accumulator.views();
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

  static v8::Handle<v8::Value> GetL10nMessage(const v8::Arguments& args) {
    if (args.Length() != 2 || !args[0]->IsString()) {
      NOTREACHED() << "Bad arguments";
      return v8::Undefined();
    }

    std::string extension_id = ExtensionIdForCurrentContext();
    if (extension_id.empty())
      return v8::Undefined();

    L10nMessagesMap* l10n_messages = GetL10nMessagesMap(extension_id);
    if (!l10n_messages)
      return v8::Undefined();

    std::string message_name = *v8::String::AsciiValue(args[0]);
    std::string message =
      ExtensionMessageBundle::GetL10nMessage(message_name, *l10n_messages);

    std::vector<std::string> substitutions;
    if (args[1]->IsNull() || args[1]->IsUndefined()) {
      // chrome.i18n.getMessage("message_name");
      // chrome.i18n.getMessage("message_name", null);
      return v8::String::New(message.c_str());
    } else if (args[1]->IsString()) {
      // chrome.i18n.getMessage("message_name", "one param");
      std::string substitute = *v8::String::Utf8Value(args[1]->ToString());
      substitutions.push_back(substitute);
    } else if (args[1]->IsArray()) {
      // chrome.i18n.getMessage("message_name", ["more", "params"]);
      v8::Array* placeholders = static_cast<v8::Array*>(*args[1]);
      uint32_t count = placeholders->Length();
      DCHECK(count > 0 && count <= 9);
      for (uint32_t i = 0; i < count; ++i) {
        std::string substitute =
          *v8::String::Utf8Value(
              placeholders->Get(v8::Integer::New(i))->ToString());
        substitutions.push_back(substitute);
      }
    } else {
      NOTREACHED() << "Couldn't parse second parameter.";
      return v8::Undefined();
    }

    return v8::String::New(ReplaceStringPlaceholders(
        message, substitutions, NULL).c_str());
  }

  // Common code for starting an API request to the browser. |value_args|
  // contains the request's arguments.
  static v8::Handle<v8::Value> StartRequestCommon(
      const v8::Arguments& args, Value* value_args) {
    // Get the current RenderView so that we can send a routed IPC message from
    // the correct source.
    RenderView* renderview = bindings_utils::GetRenderViewForCurrentContext();
    if (!renderview)
      return v8::Undefined();

    std::string name = *v8::String::AsciiValue(args[0]);
    if (GetFunctionNameSet()->find(name) == GetFunctionNameSet()->end()) {
      NOTREACHED() << "Unexpected function " << name;
      return v8::Undefined();
    }

    if (!ExtensionProcessBindings::CurrentContextHasPermission(name)) {
      return ExtensionProcessBindings::ThrowPermissionDeniedException(name);
    }

    int request_id = args[2]->Int32Value();
    bool has_callback = args[3]->BooleanValue();

    // Put the args in a 1-element list for easier serialization. Maybe all
    // requests should have a list of args?
    ListValue args_holder;
    args_holder.Append(value_args);

    v8::Persistent<v8::Context> current_context =
        v8::Persistent<v8::Context>::New(v8::Context::GetCurrent());
    DCHECK(!current_context.IsEmpty());
    GetPendingRequestMap()[request_id].reset(new PendingRequest(
        current_context, name));

    renderview->SendExtensionRequest(name, args_holder,
                                     request_id, has_callback);

    return v8::Undefined();
  }

  // Starts an API request to the browser, with an optional callback.  The
  // callback will be dispatched to EventBindings::HandleResponse.
  static v8::Handle<v8::Value> StartRequest(const v8::Arguments& args) {
    std::string str_args = *v8::String::Utf8Value(args[1]);
    base::JSONReader reader;
    Value* value_args = reader.JsonToValue(str_args, false, false);

    // Since we do the serialization in the v8 extension, we should always get
    // valid JSON.
    if (!value_args) {
      NOTREACHED() << "Invalid JSON passed to StartRequest.";
      return v8::Undefined();
    }

    return StartRequestCommon(args, value_args);
  }

  // A special request for setting the extension action icon. This function
  // accepts a canvas ImageData object, so it needs to do extra processing
  // before sending the request to the browser.
  static v8::Handle<v8::Value> SetExtensionActionIcon(
      const v8::Arguments& args) {
    v8::Local<v8::Object> details = args[1]->ToObject();
    v8::Local<v8::Object> image_data =
        details->Get(v8::String::New("imageData"))->ToObject();
    v8::Local<v8::Object> data =
        image_data->Get(v8::String::New("data"))->ToObject();
    int width = image_data->Get(v8::String::New("width"))->Int32Value();
    int height = image_data->Get(v8::String::New("height"))->Int32Value();

    int data_length = data->Get(v8::String::New("length"))->Int32Value();
    if (data_length != 4 * width * height) {
      NOTREACHED() << "Invalid argument to setIcon. Expecting ImageData.";
      return v8::Undefined();
    }

    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);
    bitmap.allocPixels();
    bitmap.eraseARGB(0, 0, 0, 0);

    uint32_t* pixels = bitmap.getAddr32(0, 0);
    for (int t = 0; t < width*height; t++) {
      // |data| is RGBA, pixels is ARGB.
      pixels[t] = SkPreMultiplyColor(
          ((data->Get(v8::Integer::New(4*t + 3))->Int32Value() & 0xFF) << 24) |
          ((data->Get(v8::Integer::New(4*t + 0))->Int32Value() & 0xFF) << 16) |
          ((data->Get(v8::Integer::New(4*t + 1))->Int32Value() & 0xFF) << 8) |
          ((data->Get(v8::Integer::New(4*t + 2))->Int32Value() & 0xFF) << 0));
    }

    // Construct the Value object.
    IPC::Message bitmap_pickle;
    IPC::WriteParam(&bitmap_pickle, bitmap);
    Value* bitmap_value = BinaryValue::CreateWithCopiedBuffer(
        static_cast<const char*>(bitmap_pickle.data()), bitmap_pickle.size());

    DictionaryValue* dict = new DictionaryValue();
    dict->Set(L"imageData", bitmap_value);

    if (details->Has(v8::String::New("tabId"))) {
      dict->SetInteger(L"tabId",
                       details->Get(v8::String::New("tabId"))->Int32Value());
    }

    return StartRequestCommon(args, dict);
  }

  static v8::Handle<v8::Value> GetRenderViewId(const v8::Arguments& args) {
    RenderView* renderview = bindings_utils::GetRenderViewForCurrentContext();
    if (!renderview)
      return v8::Undefined();
    return v8::Integer::New(renderview->routing_id());
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
void ExtensionProcessBindings::SetL10nMessages(
    const std::string& extension_id,
    const std::map<std::string, std::string>& l10n_messages) {
  ExtensionToL10nMessagesMap& l10n_messages_map =
    *GetExtensionToL10nMessagesMap();
  l10n_messages_map[extension_id] = l10n_messages;
}

// static
void ExtensionProcessBindings::SetAPIPermissions(
    const std::string& extension_id,
    const std::vector<std::string>& permissions) {
  PermissionsMap& permissions_map = *GetPermissionsMap(extension_id);

  // Default all the API permissions to off. We will reset them below.
  for (size_t i = 0; i < Extension::kNumPermissions; ++i)
    permissions_map[Extension::kPermissionNames[i]] = false;
  for (size_t i = 0; i < permissions.size(); ++i)
    permissions_map[permissions[i]] = true;
}

// static
void ExtensionProcessBindings::SetHostPermissions(
    const GURL& extension_url,
    const std::vector<URLPattern>& permissions) {
  for (size_t i = 0; i < permissions.size(); ++i) {
    WebSecurityPolicy::whiteListAccessFromOrigin(
        extension_url,
        WebKit::WebString::fromUTF8(permissions[i].scheme()),
        WebKit::WebString::fromUTF8(permissions[i].host()),
        permissions[i].match_subdomains());
  }
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
  // some APIs do not have a required permission entry (ie, "chrome.extension").
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

  return v8::ThrowException(v8::Exception::Error(
      v8::String::New(error_msg.c_str())));
}

// static
void ExtensionProcessBindings::SetViewType(WebView* view,
                                           ViewType::Type type) {
  DCHECK(type == ViewType::EXTENSION_MOLE ||
         type == ViewType::EXTENSION_TOOLSTRIP);
  const char* type_str;
  if (type == ViewType::EXTENSION_MOLE)
    type_str = "mole";
  else if (type == ViewType::EXTENSION_TOOLSTRIP)
    type_str = "toolstrip";
  else
    return;

  v8::HandleScope handle_scope;
  WebFrame* frame = view->mainFrame();
  v8::Local<v8::Context> context = frame->mainWorldScriptContext();
  v8::Handle<v8::Value> argv[1];
  argv[0] = v8::String::New(type_str);
  bindings_utils::CallFunctionInContext(context, "setViewType",
                                        arraysize(argv), argv);
}
