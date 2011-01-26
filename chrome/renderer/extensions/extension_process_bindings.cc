// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_process_bindings.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/url_pattern.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/extensions/bindings_utils.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/extensions/extension_renderer_info.h"
#include "chrome/renderer/extensions/js_only_v8_extensions.h"
#include "chrome/renderer/extensions/renderer_extension_bindings.h"
#include "chrome/renderer/user_script_slave.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/render_view_visitor.h"
#include "grit/common_resources.h"
#include "grit/renderer_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityPolicy.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

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

// A list of permissions that are enabled for this extension.
typedef std::set<std::string> PermissionsList;

// A map of extension ID to permissions map.
typedef std::map<std::string, PermissionsList> ExtensionPermissionsList;

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
  ExtensionPermissionsList permissions_;
};

static base::LazyInstance<SingletonData> g_singleton_data(
    base::LINKER_INITIALIZED);

static std::set<std::string>* GetFunctionNameSet() {
  return &g_singleton_data.Get().function_names_;
}

static PageActionIdMap* GetPageActionMap() {
  return &g_singleton_data.Get().page_action_ids_;
}

static PermissionsList* GetPermissionsList(const std::string& extension_id) {
  return &g_singleton_data.Get().permissions_[extension_id];
}

static void GetActiveExtensionIDs(std::set<std::string>* extension_ids) {
  ExtensionPermissionsList& permissions = g_singleton_data.Get().permissions_;

  for (ExtensionPermissionsList::iterator iter = permissions.begin();
       iter != permissions.end(); ++iter) {
    extension_ids->insert(iter->first);
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

    // If we are searching for a pop-up, it may be the case that the pop-up
    // is not attached to a browser window instance.  (It is hosted in a
    // ExternalTabContainer.)  If so, then bypass validation of
    // same-browser-window origin.
    // TODO(twiz):  The browser window id of the views visited should always
    // match that of the arguments to the accumulator.
    // See bug:  http://crbug.com/29646
    if (!(view_type_ == ViewType::EXTENSION_POPUP &&
          render_view->browser_window_id() ==
               extension_misc::kUnknownWindowId)) {
      if (browser_window_id_ != extension_misc::kUnknownWindowId &&
          render_view->browser_window_id() != browser_window_id_) {
        return true;
      }
    }

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
      kExtensionName, GetStringResource(IDR_EXTENSION_PROCESS_BINDINGS_JS),
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
    const ExtensionRendererInfo* extensions =
        EventBindings::GetRenderThread()->GetExtensions();
    if (!extensions->ExtensionBindingsAllowed(url))
      return std::string();

    return extensions->GetIdByURL(url);
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
    } else if (name->Equals(v8::String::New("GetPopupView"))) {
      return v8::FunctionTemplate::New(GetPopupView);
    } else if (name->Equals(v8::String::New("GetPopupParentWindow"))) {
      return v8::FunctionTemplate::New(GetPopupParentWindow);
    } else if (name->Equals(v8::String::New("SetIconCommon"))) {
      return v8::FunctionTemplate::New(SetIconCommon);
    } else if (name->Equals(v8::String::New("IsExtensionProcess"))) {
      return v8::FunctionTemplate::New(IsExtensionProcess);
    } else if (name->Equals(v8::String::New("IsIncognitoProcess"))) {
      return v8::FunctionTemplate::New(IsIncognitoProcess);
    }

    return ExtensionBase::GetNativeFunction(name);
  }

 private:
  static v8::Handle<v8::Value> GetExtensionAPIDefinition(
      const v8::Arguments& args) {
    return v8::String::New(GetStringResource(IDR_EXTENSION_API_JSON));
  }

  static v8::Handle<v8::Value> PopupViewFinder(
      const v8::Arguments& args,
      ViewType::Type viewtype_to_find) {
    // TODO(twiz)  Correct the logic that ties the ownership of the pop-up view
    // to the hosting view.  At the moment we assume that there may only be
    // a single pop-up view for a given extension.  By doing so, we can find
    // the pop-up view by simply searching for the only pop-up view present.
    // We also assume that if the current view is a pop-up, we can find the
    // hosting view by searching for a tab contents view.
    if (args.Length() != 0)
      return v8::Undefined();

    if (viewtype_to_find != ViewType::EXTENSION_POPUP &&
        viewtype_to_find != ViewType::EXTENSION_INFOBAR &&
        viewtype_to_find != ViewType::TAB_CONTENTS) {
      NOTREACHED() << "Requesting invalid view type.";
    }

    // Disallow searching for the same view type as the current view:
    // Popups can only look for hosts, and hosts can only look for popups.
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
    DCHECK(1 == popup_matcher.views()->Length());

    // Return the first view found.
    return popup_matcher.views()->Get(v8::Integer::New(0));
  }

  static v8::Handle<v8::Value> GetPopupView(const v8::Arguments& args) {
    return PopupViewFinder(args, ViewType::EXTENSION_POPUP);
  }

  static v8::Handle<v8::Value> GetPopupParentWindow(const v8::Arguments& args) {
    v8::Handle<v8::Value> view = PopupViewFinder(args, ViewType::TAB_CONTENTS);
    if (view == v8::Undefined()) {
      view = PopupViewFinder(args, ViewType::EXTENSION_INFOBAR);
    }
    return view;
  }

  static v8::Handle<v8::Value> GetExtensionViews(const v8::Arguments& args) {
    if (args.Length() != 2)
      return v8::Undefined();

    if (!args[0]->IsInt32() || !args[1]->IsString())
      return v8::Undefined();

    // |browser_window_id| == extension_misc::kUnknownWindowId means getting
    // views attached to any browser window.
    int browser_window_id = args[0]->Int32Value();

    std::string view_type_string = *v8::String::Utf8Value(args[1]->ToString());
    StringToUpperASCII(&view_type_string);
    // |view_type| == ViewType::INVALID means getting any type of views.
    ViewType::Type view_type = ViewType::INVALID;
    if (view_type_string == ViewType::kBackgroundPage) {
      view_type = ViewType::EXTENSION_BACKGROUND_PAGE;
    } else if (view_type_string == ViewType::kInfobar) {
      view_type = ViewType::EXTENSION_INFOBAR;
    } else if (view_type_string == ViewType::kNotification) {
      view_type = ViewType::NOTIFICATION;
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

  // Common code for starting an API request to the browser. |value_args|
  // contains the request's arguments.
  // Steals value_args contents for efficiency.
  static v8::Handle<v8::Value> StartRequestCommon(
      const v8::Arguments& args, ListValue* value_args) {
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

    GURL source_url;
    WebFrame* webframe = WebFrame::frameForCurrentContext();
    if (webframe)
      source_url = webframe->url();

    int request_id = args[2]->Int32Value();
    bool has_callback = args[3]->BooleanValue();

    v8::Persistent<v8::Context> current_context =
        v8::Persistent<v8::Context>::New(v8::Context::GetCurrent());
    DCHECK(!current_context.IsEmpty());
    GetPendingRequestMap()[request_id].reset(new PendingRequest(
        current_context, name));

    ViewHostMsg_DomMessage_Params params;
    params.name = name;
    params.arguments.Swap(value_args);
    params.source_url = source_url;
    params.request_id = request_id;
    params.has_callback = has_callback;
    params.user_gesture = webframe->isProcessingUserGesture();
    renderview->SendExtensionRequest(params);

    return v8::Undefined();
  }

  // Starts an API request to the browser, with an optional callback.  The
  // callback will be dispatched to EventBindings::HandleResponse.
  static v8::Handle<v8::Value> StartRequest(const v8::Arguments& args) {
    std::string str_args = *v8::String::Utf8Value(args[1]);
    base::JSONReader reader;
    scoped_ptr<Value> value_args;
    value_args.reset(reader.JsonToValue(str_args, false, false));

    // Since we do the serialization in the v8 extension, we should always get
    // valid JSON.
    if (!value_args.get() || !value_args->IsType(Value::TYPE_LIST)) {
      NOTREACHED() << "Invalid JSON passed to StartRequest.";
      return v8::Undefined();
    }

    return StartRequestCommon(args, static_cast<ListValue*>(value_args.get()));
  }

  static bool ConvertImageDataToBitmapValue(
      const v8::Arguments& args, Value** bitmap_value) {
    v8::Local<v8::Object> extension_args = args[1]->ToObject();
    v8::Local<v8::Object> details =
        extension_args->Get(v8::String::New("0"))->ToObject();
    v8::Local<v8::Object> image_data =
        details->Get(v8::String::New("imageData"))->ToObject();
    v8::Local<v8::Object> data =
        image_data->Get(v8::String::New("data"))->ToObject();
    int width = image_data->Get(v8::String::New("width"))->Int32Value();
    int height = image_data->Get(v8::String::New("height"))->Int32Value();

    int data_length = data->Get(v8::String::New("length"))->Int32Value();
    if (data_length != 4 * width * height) {
      NOTREACHED() << "Invalid argument to setIcon. Expecting ImageData.";
      return false;
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
    *bitmap_value = BinaryValue::CreateWithCopiedBuffer(
        static_cast<const char*>(bitmap_pickle.data()), bitmap_pickle.size());

    return true;
  }

  // A special request for setting the extension action icon and the sidebar
  // mini tab icon. This function accepts a canvas ImageData object, so it needs
  // to do extra processing before sending the request to the browser.
  static v8::Handle<v8::Value> SetIconCommon(
      const v8::Arguments& args) {
    Value* bitmap_value = NULL;
    if (!ConvertImageDataToBitmapValue(args, &bitmap_value))
      return v8::Undefined();

    v8::Local<v8::Object> extension_args = args[1]->ToObject();
    v8::Local<v8::Object> details =
        extension_args->Get(v8::String::New("0"))->ToObject();

    DictionaryValue* dict = new DictionaryValue();
    dict->Set("imageData", bitmap_value);

    if (details->Has(v8::String::New("tabId"))) {
      dict->SetInteger("tabId",
                       details->Get(v8::String::New("tabId"))->Int32Value());
    }

    ListValue list_value;
    list_value.Append(dict);

    return StartRequestCommon(args, &list_value);
  }

  static v8::Handle<v8::Value> GetRenderViewId(const v8::Arguments& args) {
    RenderView* renderview = bindings_utils::GetRenderViewForCurrentContext();
    if (!renderview)
      return v8::Undefined();
    return v8::Integer::New(renderview->routing_id());
  }

  static v8::Handle<v8::Value> IsExtensionProcess(const v8::Arguments& args) {
    bool retval = false;
    if (EventBindings::GetRenderThread())
      retval = EventBindings::GetRenderThread()->IsExtensionProcess();
    return v8::Boolean::New(retval);
  }

  static v8::Handle<v8::Value> IsIncognitoProcess(const v8::Arguments& args) {
    bool retval = false;
    if (EventBindings::GetRenderThread())
      retval = EventBindings::GetRenderThread()->IsIncognitoProcess();
    return v8::Boolean::New(retval);
  }
};

}  // namespace

v8::Extension* ExtensionProcessBindings::Get() {
  static v8::Extension* extension = new ExtensionImpl();
  return extension;
}

void ExtensionProcessBindings::GetActiveExtensions(
    std::set<std::string>* extension_ids) {
  GetActiveExtensionIDs(extension_ids);
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
#ifndef NDEBUG
  if (!retval.IsEmpty() && !retval->IsUndefined()) {
    std::string error = *v8::String::AsciiValue(retval);
    DCHECK(false) << error;
  }
#endif

  request->second->context.Dispose();
  request->second->context.Clear();
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
void ExtensionProcessBindings::SetAPIPermissions(
    const std::string& extension_id,
    const std::set<std::string>& permissions) {
  PermissionsList& permissions_list = *GetPermissionsList(extension_id);
  permissions_list.clear();
  permissions_list.insert(permissions.begin(), permissions.end());
}

// static
void ExtensionProcessBindings::SetHostPermissions(
    const GURL& extension_url,
    const std::vector<URLPattern>& permissions) {
  for (size_t i = 0; i < permissions.size(); ++i) {
    const char* schemes[] = {
      chrome::kHttpScheme,
      chrome::kHttpsScheme,
      chrome::kFileScheme,
      chrome::kChromeUIScheme,
    };
    for (size_t j = 0; j < arraysize(schemes); ++j) {
      if (permissions[i].MatchesScheme(schemes[j])) {
        WebSecurityPolicy::addOriginAccessWhitelistEntry(
            extension_url,
            WebKit::WebString::fromUTF8(schemes[j]),
            WebKit::WebString::fromUTF8(permissions[i].host()),
            permissions[i].match_subdomains());
      }
    }
  }
}

// static
bool ExtensionProcessBindings::CurrentContextHasPermission(
    const std::string& function_name) {
  std::string extension_id = ExtensionImpl::ExtensionIdForCurrentContext();
  return HasPermission(extension_id, function_name);
}

// static
bool ExtensionProcessBindings::HasPermission(const std::string& extension_id,
                                             const std::string& permission) {
  PermissionsList& permissions_list = *GetPermissionsList(extension_id);
  return Extension::HasApiPermission(permissions_list, permission);
}

// static
v8::Handle<v8::Value>
    ExtensionProcessBindings::ThrowPermissionDeniedException(
      const std::string& function_name) {
  static const char kMessage[] =
      "You do not have permission to use '%s'. Be sure to declare"
      " in your manifest what permissions you need.";
  std::string error_msg = StringPrintf(kMessage, function_name.c_str());

  return v8::ThrowException(v8::Exception::Error(
      v8::String::New(error_msg.c_str())));
}
