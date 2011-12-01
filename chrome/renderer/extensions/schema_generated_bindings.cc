// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/schema_generated_bindings.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/extensions/url_pattern.h"
#include "chrome/common/extensions/api/extension_api.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/chrome_view_type.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_visitor.h"
#include "chrome/renderer/chrome_render_process_observer.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/chrome_v8_context_set.h"
#include "chrome/renderer/extensions/chrome_v8_extension.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "chrome/renderer/extensions/extension_helper.h"
#include "chrome/renderer/extensions/miscellaneous_bindings.h"
#include "chrome/renderer/extensions/user_script_slave.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/v8_value_converter.h"
#include "grit/common_resources.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBlob.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/resource/resource_bundle.h"
#include "v8/include/v8.h"
#include "webkit/glue/webkit_glue.h"

using content::V8ValueConverter;
using extensions::ExtensionAPI;
using WebKit::WebFrame;
using WebKit::WebView;

namespace {

const char* kExtensionDeps[] = {
  "extensions/event.js",
  "extensions/json_schema.js",
  "extensions/miscellaneous_bindings.js",
  "extensions/apitest.js"
};

// Contains info relevant to a pending API request.
struct PendingRequest {
 public :
  PendingRequest(v8::Persistent<v8::Context> context, const std::string& name,
                 const std::string& extension_id)
      : context(context), name(name), extension_id(extension_id) {
  }
  v8::Persistent<v8::Context> context;
  std::string name;
  std::string extension_id;
};
typedef std::map<int, linked_ptr<PendingRequest> > PendingRequestMap;

base::LazyInstance<PendingRequestMap> g_pending_requests =
    LAZY_INSTANCE_INITIALIZER;

// A RenderViewVisitor class that iterates through the set of available
// views, looking for a view of the given type, in the given browser window
// and within the given extension.
// Used to accumulate the list of views associated with an extension.
class ExtensionViewAccumulator : public content::RenderViewVisitor {
 public:
  ExtensionViewAccumulator(const std::string& extension_id,
                           int browser_window_id,
                           content::ViewType view_type)
      : extension_id_(extension_id),
        browser_window_id_(browser_window_id),
        view_type_(view_type),
        views_(v8::Array::New()),
        index_(0) {
  }

  v8::Local<v8::Array> views() { return views_; }

  virtual bool Visit(content::RenderView* render_view) {
    ExtensionHelper* helper = ExtensionHelper::Get(render_view);
    if (!ViewTypeMatches(helper->view_type(), view_type_))
      return true;

    GURL url = render_view->GetWebView()->mainFrame()->document().url();
    if (!url.SchemeIs(chrome::kExtensionScheme))
      return true;
    const std::string& extension_id = url.host();
    if (extension_id != extension_id_)
      return true;

    if (browser_window_id_ != extension_misc::kUnknownWindowId &&
        helper->browser_window_id() != browser_window_id_) {
      return true;
    }

    v8::Local<v8::Context> context =
        render_view->GetWebView()->mainFrame()->mainWorldScriptContext();
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
  bool OnMatchedView(v8::Local<v8::Value> view_window) {
    views_->Set(v8::Integer::New(index_), view_window);
    index_++;

    if (view_type_ == chrome::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE)
      return false;  // There can be only one...

    return true;
  }

  // Returns true is |type| "isa" |match|.
  static bool ViewTypeMatches(content::ViewType type, content::ViewType match) {
    if (type == match)
      return true;

    // INVALID means match all.
    if (match == content::VIEW_TYPE_INVALID)
      return true;

    return false;
  }

  std::string extension_id_;
  int browser_window_id_;
  content::ViewType view_type_;
  v8::Local<v8::Array> views_;
  int index_;
};

class ExtensionImpl : public ChromeV8Extension {
 public:
  explicit ExtensionImpl(ExtensionDispatcher* extension_dispatcher)
      : ChromeV8Extension("extensions/schema_generated_bindings.js",
                          IDR_SCHEMA_GENERATED_BINDINGS_JS,
                          arraysize(kExtensionDeps),
                          kExtensionDeps,
                          extension_dispatcher) {
  }

  ~ExtensionImpl() {
    // TODO(aa): It seems that v8 never deletes us, so this doesn't get called.
    // Leaving this in here in case v8's implementation ever changes.
    if (!extension_api_.IsEmpty())
      extension_api_.Dispose();
  }

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) OVERRIDE {
    if (name->Equals(v8::String::New("GetExtensionAPIDefinition"))) {
      return v8::FunctionTemplate::New(GetExtensionAPIDefinition,
                                       v8::External::New(this));
    } else if (name->Equals(v8::String::New("GetExtensionViews"))) {
      return v8::FunctionTemplate::New(GetExtensionViews,
                                       v8::External::New(this));
    } else if (name->Equals(v8::String::New("GetNextRequestId"))) {
      return v8::FunctionTemplate::New(GetNextRequestId);
    } else if (name->Equals(v8::String::New("OpenChannelToTab"))) {
      return v8::FunctionTemplate::New(OpenChannelToTab);
    } else if (name->Equals(v8::String::New("GetNextContextMenuId"))) {
      return v8::FunctionTemplate::New(GetNextContextMenuId);
    } else if (name->Equals(v8::String::New("GetNextTtsEventId"))) {
      return v8::FunctionTemplate::New(GetNextTtsEventId);
    } else if (name->Equals(v8::String::New("GetCurrentPageActions"))) {
      return v8::FunctionTemplate::New(GetCurrentPageActions,
                                       v8::External::New(this));
    } else if (name->Equals(v8::String::New("StartRequest"))) {
      return v8::FunctionTemplate::New(StartRequest,
                                       v8::External::New(this));
    } else if (name->Equals(v8::String::New("GetRenderViewId"))) {
      return v8::FunctionTemplate::New(GetRenderViewId);
    } else if (name->Equals(v8::String::New("SetIconCommon"))) {
      return v8::FunctionTemplate::New(SetIconCommon,
                                       v8::External::New(this));
    } else if (name->Equals(v8::String::New("GetUniqueSubEventName"))) {
      return v8::FunctionTemplate::New(GetUniqueSubEventName);
    } else if (name->Equals(v8::String::New("GetLocalFileSystem"))) {
      return v8::FunctionTemplate::New(GetLocalFileSystem);
    } else if (name->Equals(v8::String::New("DecodeJPEG"))) {
      return v8::FunctionTemplate::New(DecodeJPEG, v8::External::New(this));
    } else if (name->Equals(v8::String::New("CreateBlob"))) {
      return v8::FunctionTemplate::New(CreateBlob, v8::External::New(this));
    } else if (name->Equals(v8::String::New("SendResponseAck"))) {
      return v8::FunctionTemplate::New(SendResponseAck,
                                       v8::External::New(this));
    }

    return ChromeV8Extension::GetNativeFunction(name);
  }

 private:
  static v8::Handle<v8::Value> GetExtensionAPIDefinition(
      const v8::Arguments& args) {
    ExtensionImpl* self = GetFromArguments<ExtensionImpl>(args);
    if (!self->extension_api_.IsEmpty())
      return self->extension_api_;

    v8::Persistent<v8::Context> context(v8::Context::New());
    v8::Context::Scope context_scope(context);

    scoped_ptr<V8ValueConverter> v8_value_converter(V8ValueConverter::create());
    self->extension_api_ = v8::Persistent<v8::Array>::New(
        v8::Handle<v8::Array>::Cast(
            v8_value_converter->ToV8Value(
                ExtensionAPI::GetInstance()->value(), context)));
    CHECK(!self->extension_api_.IsEmpty());

    // The persistent extension_api_ will keep the context alive.
    context.Dispose();

    return self->extension_api_;
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
    // |view_type| == content::VIEW_TYPE_INVALID means getting any type of
    // views.
    content::ViewType view_type = content::VIEW_TYPE_INVALID;
    if (view_type_string == chrome::kViewTypeBackgroundPage) {
      view_type = chrome::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE;
    } else if (view_type_string == chrome::kViewTypeInfobar) {
      view_type = chrome::VIEW_TYPE_EXTENSION_INFOBAR;
    } else if (view_type_string == chrome::kViewTypeNotification) {
      view_type = chrome::VIEW_TYPE_NOTIFICATION;
    } else if (view_type_string == chrome::kViewTypeTabContents) {
      view_type = content::VIEW_TYPE_TAB_CONTENTS;
    } else if (view_type_string == chrome::kViewTypePopup) {
      view_type = chrome::VIEW_TYPE_EXTENSION_POPUP;
    } else if (view_type_string == chrome::kViewTypeExtensionDialog) {
      view_type = chrome::VIEW_TYPE_EXTENSION_DIALOG;
    } else if (view_type_string != chrome::kViewTypeAll) {
      return v8::Undefined();
    }

    ExtensionImpl* v8_extension = GetFromArguments<ExtensionImpl>(args);
    const ::Extension* extension =
        v8_extension->GetExtensionForCurrentRenderView();
    if (!extension)
      return v8::Undefined();

    ExtensionViewAccumulator accumulator(extension->id(), browser_window_id,
                                         view_type);
    content::RenderView::ForEach(&accumulator);
    return accumulator.views();
  }

  static v8::Handle<v8::Value> GetNextRequestId(const v8::Arguments& args) {
    static int next_request_id = 0;
    return v8::Integer::New(next_request_id++);
  }

  // Attach an event name to an object.
  static v8::Handle<v8::Value> GetUniqueSubEventName(
      const v8::Arguments& args) {
    static int next_event_id = 0;
    DCHECK(args.Length() == 1);
    DCHECK(args[0]->IsString());
    std::string event_name(*v8::String::AsciiValue(args[0]));
    std::string unique_event_name =
        event_name + "/" + base::IntToString(++next_event_id);
    return v8::String::New(unique_event_name.c_str());
  }

  static v8::Handle<v8::Value> GetLocalFileSystem(
      const v8::Arguments& args) {
    DCHECK(args.Length() == 2);
    DCHECK(args[0]->IsString());
    DCHECK(args[1]->IsString());
    std::string name(*v8::String::Utf8Value(args[0]));
    std::string path(*v8::String::Utf8Value(args[1]));

    WebFrame* webframe = WebFrame::frameForCurrentContext();
    return webframe->createFileSystem(WebKit::WebFileSystem::TypeExternal,
            WebKit::WebString::fromUTF8(name.c_str()),
            WebKit::WebString::fromUTF8(path.c_str()));
  }

  // Decodes supplied JPEG byte array to image pixel array.
  static v8::Handle<v8::Value> DecodeJPEG(const v8::Arguments& args) {
    static const char* kAllowedIds[] = {
        "haiffjcadagjlijoggckpgfnoeiflnem",
        "gnedhmakppccajfpfiihfcdlnpgomkcf",
        "fjcibdnjlbfnbfdjneajpipnlcppleek",
        "oflbaaikkabfdfkimeclgkackhdkpnip"  // Testing extension.
    };
    const std::vector<std::string> allowed_ids(
        kAllowedIds, kAllowedIds + arraysize(kAllowedIds));

    ExtensionImpl* v8_extension = GetFromArguments<ExtensionImpl>(args);
    const ::Extension* extension =
        v8_extension->GetExtensionForCurrentRenderView();
    if (!extension)
      return v8::Undefined();
    if (allowed_ids.end() == std::find(
          allowed_ids.begin(), allowed_ids.end(), extension->id())) {
      return v8::Undefined();
    }

    DCHECK(args.Length() == 1);
    DCHECK(args[0]->IsArray());
    v8::Local<v8::Object> jpeg_array = args[0]->ToObject();
    size_t jpeg_length =
        jpeg_array->Get(v8::String::New("length"))->Int32Value();

    // Put input JPEG array into string for DecodeImage().
    std::string jpeg_array_string;
    jpeg_array_string.reserve(jpeg_length);

    // Unfortunately we cannot request continuous backing store of the
    // |jpeg_array| object as it might not have one. So we make
    // element copy here.
    // Note(mnaganov): If it is not fast enough
    // (and main constraints might be in repetition of v8 API calls),
    // change the argument type from Array to String and use
    // String::Write().
    // Note(vitalyr): Another option is to use Int8Array for inputs and
    // Int32Array for output.
    for (size_t i = 0; i != jpeg_length; ++i) {
      jpeg_array_string.push_back(
          jpeg_array->Get(v8::Integer::New(i))->Int32Value());
    }

    // Decode and verify resulting image metrics.
    SkBitmap bitmap;
    if (!webkit_glue::DecodeImage(jpeg_array_string, &bitmap))
      return v8::Undefined();
    if (bitmap.config() != SkBitmap::kARGB_8888_Config)
      return v8::Undefined();
    const int width = bitmap.width();
    const int height = bitmap.height();
    SkAutoLockPixels lockpixels(bitmap);
    const uint32_t* pixels = static_cast<uint32_t*>(bitmap.getPixels());
    if (!pixels)
      return v8::Undefined();

    // Compose output array. This API call only accepts kARGB_8888_Config images
    // so we rely on each pixel occupying 4 bytes.
    // Note(mnaganov): to speed this up, you may use backing store
    // technique from CreateExternalArray() of v8/src/d8.cc.
    v8::Local<v8::Array> bitmap_array(v8::Array::New(width * height));
    for (int i = 0; i != width * height; ++i) {
      bitmap_array->Set(v8::Integer::New(i),
                        v8::Integer::New(pixels[i] & 0xFFFFFF));
    }
    return bitmap_array;
  }

  // Creates a Blob with the content of the specified file.
  static v8::Handle<v8::Value> CreateBlob(const v8::Arguments& args) {
    CHECK(args.Length() == 2);
    CHECK(args[0]->IsString());
    CHECK(args[1]->IsInt32());
    WebKit::WebString path(UTF8ToUTF16(*v8::String::Utf8Value(args[0])));
    WebKit::WebBlob blob =
        WebKit::WebBlob::createFromFile(path, args[1]->Int32Value());
    return blob.toV8Value();
  }

  static v8::Handle<v8::Value> SendResponseAck(const v8::Arguments& args) {
    CHECK(args.Length() == 1);
    CHECK(args[0]->IsInt32());

    content::RenderView* render_view = GetCurrentRenderView();
    if (render_view) {
      render_view->Send(new ExtensionHostMsg_ResponseAck(
          render_view->GetRoutingId(), args[0]->Int32Value()));
    }
    return v8::Undefined();
  }

  // Creates a new messaging channel to the tab with the given ID.
  static v8::Handle<v8::Value> OpenChannelToTab(const v8::Arguments& args) {
    // Get the current RenderView so that we can send a routed IPC message from
    // the correct source.
    content::RenderView* renderview = GetCurrentRenderView();
    if (!renderview)
      return v8::Undefined();

    if (args.Length() >= 3 && args[0]->IsInt32() && args[1]->IsString() &&
        args[2]->IsString()) {
      int tab_id = args[0]->Int32Value();
      std::string extension_id = *v8::String::Utf8Value(args[1]->ToString());
      std::string channel_name = *v8::String::Utf8Value(args[2]->ToString());
      int port_id = -1;
      renderview->Send(new ExtensionHostMsg_OpenChannelToTab(
        renderview->GetRoutingId(), tab_id, extension_id, channel_name,
          &port_id));
      return v8::Integer::New(port_id);
    }
    return v8::Undefined();
  }

  static v8::Handle<v8::Value> GetNextContextMenuId(const v8::Arguments& args) {
    // Note: this works because contextMenus.create() only works in the
    // extension process.  If that API is opened up to content scripts, this
    // will need to change.  See crbug.com/77023
    static int next_context_menu_id = 1;
    return v8::Integer::New(next_context_menu_id++);
  }

  static v8::Handle<v8::Value> GetNextTtsEventId(const v8::Arguments& args) {
    // Note: this works because the TTS API only works in the
    // extension process, not content scripts.
    static int next_tts_event_id = 1;
    return v8::Integer::New(next_tts_event_id++);
  }

  static v8::Handle<v8::Value> GetCurrentPageActions(
      const v8::Arguments& args) {
    ExtensionImpl* v8_extension = GetFromArguments<ExtensionImpl>(args);
    std::string extension_id = *v8::String::Utf8Value(args[0]->ToString());
    const ::Extension* extension =
        v8_extension->extension_dispatcher_->extensions()->GetByID(
            extension_id);
    CHECK(!extension_id.empty());
    if (!extension)
      return v8::Undefined();

    v8::Local<v8::Array> page_action_vector = v8::Array::New();
    if (extension->page_action()) {
      std::string id = extension->page_action()->id();
      page_action_vector->Set(v8::Integer::New(0),
                              v8::String::New(id.c_str(), id.size()));
    }

    return page_action_vector;
  }

  // Common code for starting an API request to the browser. |value_args|
  // contains the request's arguments.
  // Steals value_args contents for efficiency.
  static v8::Handle<v8::Value> StartRequestCommon(
      const v8::Arguments& args, ListValue* value_args) {
    ExtensionImpl* v8_extension = GetFromArguments<ExtensionImpl>(args);

    const ChromeV8ContextSet& contexts =
        v8_extension->extension_dispatcher()->v8_context_set();
    ChromeV8Context* current_context = contexts.GetCurrent();
    if (!current_context)
      return v8::Undefined();

    // Get the current RenderView so that we can send a routed IPC message from
    // the correct source.
    content::RenderView* renderview = current_context->GetRenderView();
    if (!renderview)
      return v8::Undefined();

    std::string name = *v8::String::AsciiValue(args[0]);
    const std::set<std::string>& function_names =
        v8_extension->extension_dispatcher_->function_names();
    if (function_names.find(name) == function_names.end()) {
      NOTREACHED() << "Unexpected function " << name;
      return v8::Undefined();
    }

    if (!v8_extension->CheckCurrentContextAccessToExtensionAPI(name))
      return v8::Undefined();

    GURL source_url;
    WebFrame* webframe = current_context->web_frame();
    if (webframe)
      source_url = webframe->document().url();

    int request_id = args[2]->Int32Value();
    bool has_callback = args[3]->BooleanValue();
    bool for_io_thread = args[4]->BooleanValue();

    v8::Persistent<v8::Context> v8_context =
        v8::Persistent<v8::Context>::New(v8::Context::GetCurrent());
    DCHECK(!v8_context.IsEmpty());
    g_pending_requests.Get()[request_id].reset(new PendingRequest(
        v8_context, name, current_context->extension_id()));

    ExtensionHostMsg_Request_Params params;
    params.name = name;
    params.arguments.Swap(value_args);
    params.extension_id = current_context->extension_id();
    params.source_url = source_url;
    params.request_id = request_id;
    params.has_callback = has_callback;
    params.user_gesture =
        webframe ? webframe->isProcessingUserGesture() : false;
    if (for_io_thread) {
      renderview->Send(new ExtensionHostMsg_RequestForIOThread(
          renderview->GetRoutingId(), params));
    } else {
      renderview->Send(new ExtensionHostMsg_Request(
          renderview->GetRoutingId(), params));
    }

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
    *bitmap_value = base::BinaryValue::CreateWithCopiedBuffer(
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
    content::RenderView* renderview = GetCurrentRenderView();
    if (!renderview)
      return v8::Undefined();
    return v8::Integer::New(renderview->GetRoutingId());
  }

  // Cached JS Array representation of extension_api.json. We store this so that
  // we don't have to parse it over and over again for every context that uses
  // it.
  v8::Persistent<v8::Array> extension_api_;
};

}  // namespace

namespace extensions {

v8::Extension* SchemaGeneratedBindings::Get(
    ExtensionDispatcher* extension_dispatcher) {
  static v8::Extension* extension = new ExtensionImpl(extension_dispatcher);
  CHECK_EQ(extension_dispatcher,
           static_cast<ExtensionImpl*>(extension)->extension_dispatcher());
  return extension;
}

// static
void SchemaGeneratedBindings::HandleResponse(const ChromeV8ContextSet& contexts,
                                             int request_id,
                                             bool success,
                                             const std::string& response,
                                             const std::string& error,
                                             std::string* extension_id) {
  PendingRequestMap::iterator request =
      g_pending_requests.Get().find(request_id);
  if (request == g_pending_requests.Get().end()) {
    // This should not be able to happen since we only remove requests when they
    // are handled.
    LOG(ERROR) << "Could not find specified request id: " << request_id;
    return;
  }

  ChromeV8Context* v8_context =
      contexts.GetByV8Context(request->second->context);
  if (!v8_context)
    return;  // The frame went away.

  v8::HandleScope handle_scope;
  v8::Handle<v8::Value> argv[5];
  argv[0] = v8::Integer::New(request_id);
  argv[1] = v8::String::New(request->second->name.c_str());
  argv[2] = v8::Boolean::New(success);
  argv[3] = v8::String::New(response.c_str());
  argv[4] = v8::String::New(error.c_str());

  v8::Handle<v8::Value> retval;
  CHECK(v8_context->CallChromeHiddenMethod("handleResponse",
                                           arraysize(argv),
                                           argv,
                                           &retval));
  // In debug, the js will validate the callback parameters and return a
  // string if a validation error has occured.
#ifndef NDEBUG
  if (!retval.IsEmpty() && !retval->IsUndefined()) {
    std::string error = *v8::String::AsciiValue(retval);
    DCHECK(false) << error;
  }
#endif

  // Save the extension id before erasing the request.
  *extension_id = request->second->extension_id;

  request->second->context.Dispose();
  request->second->context.Clear();
  g_pending_requests.Get().erase(request);
}

// static
bool SchemaGeneratedBindings::HasPendingRequests(
    const std::string& extension_id) {
  for (PendingRequestMap::const_iterator it = g_pending_requests.Get().begin();
       it != g_pending_requests.Get().end(); ++it) {
    if (it->second->extension_id == extension_id)
      return true;
  }
  return false;
}

}  // namespace
