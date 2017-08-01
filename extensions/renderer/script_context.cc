// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/script_context.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "content/public/child/v8_value_converter.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/manifest_handlers/sandboxed_page_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "extensions/renderer/v8_helpers.h"
#include "gin/per_context_data.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebDocumentLoader.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"

namespace extensions {

namespace {

std::string GetContextTypeDescriptionString(Feature::Context context_type) {
  switch (context_type) {
    case Feature::UNSPECIFIED_CONTEXT:
      return "UNSPECIFIED";
    case Feature::BLESSED_EXTENSION_CONTEXT:
      return "BLESSED_EXTENSION";
    case Feature::UNBLESSED_EXTENSION_CONTEXT:
      return "UNBLESSED_EXTENSION";
    case Feature::CONTENT_SCRIPT_CONTEXT:
      return "CONTENT_SCRIPT";
    case Feature::WEB_PAGE_CONTEXT:
      return "WEB_PAGE";
    case Feature::BLESSED_WEB_PAGE_CONTEXT:
      return "BLESSED_WEB_PAGE";
    case Feature::WEBUI_CONTEXT:
      return "WEBUI";
    case Feature::SERVICE_WORKER_CONTEXT:
      return "SERVICE_WORKER";
    case Feature::LOCK_SCREEN_EXTENSION_CONTEXT:
      return "LOCK_SCREEN_EXTENSION";
  }
  NOTREACHED();
  return std::string();
}

static std::string ToStringOrDefault(
    const v8::Local<v8::String>& v8_string,
    const std::string& dflt) {
  if (v8_string.IsEmpty())
    return dflt;
  std::string ascii_value = *v8::String::Utf8Value(v8_string);
  return ascii_value.empty() ? dflt : ascii_value;
}

}  // namespace

// A gin::Runner that delegates to its ScriptContext.
class ScriptContext::Runner : public gin::Runner {
 public:
  explicit Runner(ScriptContext* context);

  // gin::Runner overrides.
  void Run(const std::string& source,
           const std::string& resource_name) override;
  v8::Local<v8::Value> Call(v8::Local<v8::Function> function,
                            v8::Local<v8::Value> receiver,
                            int argc,
                            v8::Local<v8::Value> argv[]) override;
  gin::ContextHolder* GetContextHolder() override;

 private:
  ScriptContext* context_;
};

ScriptContext::ScriptContext(const v8::Local<v8::Context>& v8_context,
                             blink::WebLocalFrame* web_frame,
                             const Extension* extension,
                             Feature::Context context_type,
                             const Extension* effective_extension,
                             Feature::Context effective_context_type)
    : is_valid_(true),
      v8_context_(v8_context->GetIsolate(), v8_context),
      web_frame_(web_frame),
      extension_(extension),
      context_type_(context_type),
      effective_extension_(effective_extension),
      effective_context_type_(effective_context_type),
      safe_builtins_(this),
      isolate_(v8_context->GetIsolate()),
      runner_(new Runner(this)) {
  VLOG(1) << "Created context:\n" << GetDebugString();
  gin::PerContextData* gin_data = gin::PerContextData::From(v8_context);
  CHECK(gin_data);
  gin_data->set_runner(runner_.get());
  if (web_frame_)
    url_ = GetAccessCheckedFrameURL(web_frame_);
}

ScriptContext::~ScriptContext() {
  VLOG(1) << "Destroyed context for extension\n"
          << "  extension id: " << GetExtensionID() << "\n"
          << "  effective extension id: "
          << (effective_extension_.get() ? effective_extension_->id() : "");
  CHECK(!is_valid_) << "ScriptContexts must be invalidated before destruction";
}

// static
bool ScriptContext::IsSandboxedPage(const GURL& url) {
  // TODO(kalman): This is checking the wrong thing. See comment in
  // HasAccessOrThrowError.
  if (url.SchemeIs(kExtensionScheme)) {
    const Extension* extension =
        RendererExtensionRegistry::Get()->GetByID(url.host());
    if (extension) {
      return SandboxedPageInfo::IsSandboxedPage(extension, url.path());
    }
  }
  return false;
}

void ScriptContext::Invalidate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(is_valid_);
  is_valid_ = false;

  // TODO(kalman): Make ModuleSystem use AddInvalidationObserver.
  // Ownership graph is a bit weird here.
  if (module_system_)
    module_system_->Invalidate();

  // Swap |invalidate_observers_| to a local variable to clear it, and to make
  // sure it's not mutated as we iterate.
  std::vector<base::Closure> observers;
  observers.swap(invalidate_observers_);
  for (const base::Closure& observer : observers) {
    observer.Run();
  }
  DCHECK(invalidate_observers_.empty())
      << "Invalidation observers cannot be added during invalidation";

  runner_.reset();
  v8_context_.Reset();
}

void ScriptContext::AddInvalidationObserver(const base::Closure& observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  invalidate_observers_.push_back(observer);
}

const std::string& ScriptContext::GetExtensionID() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return extension_.get() ? extension_->id() : base::EmptyString();
}

content::RenderFrame* ScriptContext::GetRenderFrame() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (web_frame_)
    return content::RenderFrame::FromWebFrame(web_frame_);
  return NULL;
}

void ScriptContext::SafeCallFunction(const v8::Local<v8::Function>& function,
                                     int argc,
                                     v8::Local<v8::Value> argv[]) {
  SafeCallFunction(function, argc, argv,
                   ScriptInjectionCallback::CompleteCallback());
}

void ScriptContext::SafeCallFunction(
    const v8::Local<v8::Function>& function,
    int argc,
    v8::Local<v8::Value> argv[],
    const ScriptInjectionCallback::CompleteCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope scope(v8_context());
  v8::MicrotasksScope microtasks(isolate(),
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Local<v8::Object> global = v8_context()->Global();
  if (web_frame_) {
    ScriptInjectionCallback* wrapper_callback = nullptr;
    if (!callback.is_null()) {
      // ScriptInjectionCallback manages its own lifetime.
      wrapper_callback = new ScriptInjectionCallback(callback);
    }
    web_frame_->RequestExecuteV8Function(v8_context(), function, global, argc,
                                         argv, wrapper_callback);
  } else {
    // TODO(devlin): This probably isn't safe.
    v8::Local<v8::Value> result = function->Call(global, argc, argv);
    if (!callback.is_null()) {
      std::vector<v8::Local<v8::Value>> results(1, result);
      callback.Run(results);
    }
  }
}

Feature::Availability ScriptContext::GetAvailability(
    const std::string& api_name) {
  return GetAvailability(api_name, CheckAliasStatus::ALLOWED);
}

Feature::Availability ScriptContext::GetAvailability(
    const std::string& api_name,
    CheckAliasStatus check_alias) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (base::StartsWith(api_name, "test", base::CompareCase::SENSITIVE)) {
    bool allowed = base::CommandLine::ForCurrentProcess()->
                       HasSwitch(::switches::kTestType);
    Feature::AvailabilityResult result =
        allowed ? Feature::IS_AVAILABLE : Feature::MISSING_COMMAND_LINE_SWITCH;
    return Feature::Availability(result,
                                 allowed ? "" : "Only allowed in tests");
  }
  // Hack: Hosted apps should have the availability of messaging APIs based on
  // the URL of the page (which might have access depending on some extension
  // with externally_connectable), not whether the app has access to messaging
  // (which it won't).
  const Extension* extension = extension_.get();
  if (extension && extension->is_hosted_app() &&
      (api_name == "runtime.connect" || api_name == "runtime.sendMessage")) {
    extension = NULL;
  }
  return ExtensionAPI::GetSharedInstance()->IsAvailable(
      api_name, extension, context_type_, url(), check_alias);
}

std::string ScriptContext::GetContextTypeDescription() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetContextTypeDescriptionString(context_type_);
}

std::string ScriptContext::GetEffectiveContextTypeDescription() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetContextTypeDescriptionString(effective_context_type_);
}

const GURL& ScriptContext::service_worker_scope() const {
  DCHECK_EQ(Feature::SERVICE_WORKER_CONTEXT, context_type());
  return service_worker_scope_;
}

bool ScriptContext::IsAnyFeatureAvailableToContext(
    const Feature& api,
    CheckAliasStatus check_alias) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(lazyboy): Decide what we should do for SERVICE_WORKER_CONTEXT, where
  // web_frame() is null.
  GURL url = web_frame() ? GetDocumentLoaderURLForFrame(web_frame()) : url_;
  return ExtensionAPI::GetSharedInstance()->IsAnyFeatureAvailableToContext(
      api, extension(), context_type(), url, check_alias);
}

// static
GURL ScriptContext::GetDocumentLoaderURLForFrame(
    const blink::WebLocalFrame* frame) {
  // Normally we would use frame->document().url() to determine the document's
  // URL, but to decide whether to inject a content script, we use the URL from
  // the data source. This "quirk" helps prevents content scripts from
  // inadvertently adding DOM elements to the compose iframe in Gmail because
  // the compose iframe's dataSource URL is about:blank, but the document URL
  // changes to match the parent document after Gmail document.writes into
  // it to create the editor.
  // http://code.google.com/p/chromium/issues/detail?id=86742
  blink::WebDocumentLoader* document_loader =
      frame->GetProvisionalDocumentLoader()
          ? frame->GetProvisionalDocumentLoader()
          : frame->GetDocumentLoader();
  return document_loader ? GURL(document_loader->GetRequest().Url()) : GURL();
}

// static
GURL ScriptContext::GetAccessCheckedFrameURL(
    const blink::WebLocalFrame* frame) {
  const blink::WebURL& weburl = frame->GetDocument().Url();
  if (weburl.IsEmpty()) {
    blink::WebDocumentLoader* document_loader =
        frame->GetProvisionalDocumentLoader()
            ? frame->GetProvisionalDocumentLoader()
            : frame->GetDocumentLoader();
    if (document_loader &&
        frame->GetSecurityOrigin().CanAccess(blink::WebSecurityOrigin::Create(
            document_loader->GetRequest().Url()))) {
      return GURL(document_loader->GetRequest().Url());
    }
  }
  return GURL(weburl);
}

// static
GURL ScriptContext::GetEffectiveDocumentURL(blink::WebLocalFrame* frame,
                                            const GURL& document_url,
                                            bool match_about_blank) {
  // Common scenario. If |match_about_blank| is false (as is the case in most
  // extensions), or if the frame is not an about:-page, just return
  // |document_url| (supposedly the URL of the frame).
  if (!match_about_blank || !document_url.SchemeIs(url::kAboutScheme))
    return document_url;

  // Non-sandboxed about:blank and about:srcdoc pages inherit their security
  // origin from their parent frame/window. So, traverse the frame/window
  // hierarchy to find the closest non-about:-page and return its URL.
  blink::WebFrame* parent = frame;
  blink::WebDocument parent_document;
  do {
    if (parent->Parent())
      parent = parent->Parent();
    else if (parent->Opener() != parent)
      parent = parent->Opener();
    else
      parent = nullptr;

    parent_document = parent && parent->IsWebLocalFrame()
                          ? parent->ToWebLocalFrame()->GetDocument()
                          : blink::WebDocument();
  } while (!parent_document.IsNull() &&
           GURL(parent_document.Url()).SchemeIs(url::kAboutScheme));

  if (!parent_document.IsNull()) {
    // Only return the parent URL if the frame can access it.
    if (frame->GetDocument().GetSecurityOrigin().CanAccess(
            parent_document.GetSecurityOrigin())) {
      return parent_document.Url();
    }
  }
  return document_url;
}

ScriptContext* ScriptContext::GetContext() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return this;
}

void ScriptContext::OnResponseReceived(const std::string& name,
                                       int request_id,
                                       bool success,
                                       const base::ListValue& response,
                                       const std::string& error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  v8::HandleScope handle_scope(isolate());

  v8::Local<v8::Value> argv[] = {
      v8::Integer::New(isolate(), request_id),
      v8::String::NewFromUtf8(isolate(), name.c_str()),
      v8::Boolean::New(isolate(), success),
      content::V8ValueConverter::Create()->ToV8Value(
          &response, v8::Local<v8::Context>::New(isolate(), v8_context_)),
      v8::String::NewFromUtf8(isolate(), error.c_str())};

  module_system()->CallModuleMethodSafe("sendRequest", "handleResponse",
                                        arraysize(argv), argv);
}

bool ScriptContext::HasAPIPermission(APIPermission::ID permission) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (effective_extension_.get()) {
    return effective_extension_->permissions_data()->HasAPIPermission(
        permission);
  }
  if (context_type() == Feature::WEB_PAGE_CONTEXT) {
    // Only web page contexts may be granted content capabilities. Other
    // contexts are either privileged WebUI or extensions with their own set of
    // permissions.
    if (content_capabilities_.find(permission) != content_capabilities_.end())
      return true;
  }
  return false;
}

bool ScriptContext::HasAccessOrThrowError(const std::string& name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Theoretically[1] we could end up with bindings being injected into
  // sandboxed frames, for example content scripts. Don't let them execute API
  // functions.
  //
  // In any case, this check is silly. The frame's document's security origin
  // already tells us if it's sandboxed. The only problem is that until
  // crbug.com/466373 is fixed, we don't know the security origin up-front and
  // may not know it here, either.
  //
  // [1] citation needed. This ScriptContext should already be in a state that
  // doesn't allow this, from ScriptContextSet::ClassifyJavaScriptContext.
  if (extension() &&
      SandboxedPageInfo::IsSandboxedPage(extension(), url_.path())) {
    static const char kMessage[] =
        "%s cannot be used within a sandboxed frame.";
    std::string error_msg = base::StringPrintf(kMessage, name.c_str());
    isolate()->ThrowException(v8::Exception::Error(
        v8::String::NewFromUtf8(isolate(), error_msg.c_str())));
    return false;
  }

  Feature::Availability availability = GetAvailability(name);
  if (!availability.is_available()) {
    isolate()->ThrowException(v8::Exception::Error(
        v8::String::NewFromUtf8(isolate(), availability.message().c_str())));
    return false;
  }

  return true;
}

std::string ScriptContext::GetDebugString() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::StringPrintf(
      "  extension id:           %s\n"
      "  frame:                  %p\n"
      "  URL:                    %s\n"
      "  context_type:           %s\n"
      "  effective extension id: %s\n"
      "  effective context type: %s",
      extension_.get() ? extension_->id().c_str() : "(none)", web_frame_,
      url_.spec().c_str(), GetContextTypeDescription().c_str(),
      effective_extension_.get() ? effective_extension_->id().c_str()
                                 : "(none)",
      GetEffectiveContextTypeDescription().c_str());
}

std::string ScriptContext::GetStackTraceAsString() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  v8::Local<v8::StackTrace> stack_trace =
      v8::StackTrace::CurrentStackTrace(isolate(), 10);
  if (stack_trace.IsEmpty() || stack_trace->GetFrameCount() <= 0) {
    return "    <no stack trace>";
  }
  std::string result;
  for (int i = 0; i < stack_trace->GetFrameCount(); ++i) {
    v8::Local<v8::StackFrame> frame = stack_trace->GetFrame(i);
    CHECK(!frame.IsEmpty());
    result += base::StringPrintf(
        "\n    at %s (%s:%d:%d)",
        ToStringOrDefault(frame->GetFunctionName(), "<anonymous>").c_str(),
        ToStringOrDefault(frame->GetScriptName(), "<anonymous>").c_str(),
        frame->GetLineNumber(), frame->GetColumn());
  }
  return result;
}

v8::Local<v8::Value> ScriptContext::RunScript(
    v8::Local<v8::String> name,
    v8::Local<v8::String> code,
    const RunScriptExceptionHandler& exception_handler) {
  DCHECK(thread_checker_.CalledOnValidThread());
  v8::EscapableHandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(v8_context());

  // Prepend extensions:: to |name| so that internal code can be differentiated
  // from external code in stack traces. This has no effect on behaviour.
  std::string internal_name =
      base::StringPrintf("extensions::%s", *v8::String::Utf8Value(name));

  if (internal_name.size() >= v8::String::kMaxLength) {
    NOTREACHED() << "internal_name is too long.";
    return v8::Undefined(isolate());
  }

  v8::MicrotasksScope microtasks(
      isolate(), v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::TryCatch try_catch(isolate());
  try_catch.SetCaptureMessage(true);
  v8::ScriptOrigin origin(
      v8_helpers::ToV8StringUnsafe(isolate(), internal_name.c_str()));
  v8::Local<v8::Script> script;
  if (!v8::Script::Compile(v8_context(), code, &origin).ToLocal(&script)) {
    exception_handler.Run(try_catch);
    return v8::Undefined(isolate());
  }

  v8::Local<v8::Value> result;
  if (!script->Run(v8_context()).ToLocal(&result)) {
    exception_handler.Run(try_catch);
    return v8::Undefined(isolate());
  }

  return handle_scope.Escape(result);
}

ScriptContext::Runner::Runner(ScriptContext* context) : context_(context) {
}

void ScriptContext::Runner::Run(const std::string& source,
                                const std::string& resource_name) {
  context_->module_system()->RunString(source, resource_name);
}

v8::Local<v8::Value> ScriptContext::Runner::Call(
    v8::Local<v8::Function> function,
    v8::Local<v8::Value> receiver,
    int argc,
    v8::Local<v8::Value> argv[]) {
  return context_->CallFunction(function, argc, argv);
}

gin::ContextHolder* ScriptContext::Runner::GetContextHolder() {
  v8::HandleScope handle_scope(context_->isolate());
  return gin::PerContextData::From(context_->v8_context())->context_holder();
}

v8::Local<v8::Value> ScriptContext::CallFunction(
    const v8::Local<v8::Function>& function,
    int argc,
    v8::Local<v8::Value> argv[]) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  v8::EscapableHandleScope handle_scope(isolate());
  v8::Context::Scope scope(v8_context());

  v8::MicrotasksScope microtasks(isolate(),
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);
  if (!is_valid_) {
    return handle_scope.Escape(
        v8::Local<v8::Primitive>(v8::Undefined(isolate())));
  }

  v8::Local<v8::Object> global = v8_context()->Global();
  if (!web_frame_)
    return handle_scope.Escape(function->Call(global, argc, argv));
  return handle_scope.Escape(
      v8::Local<v8::Value>(web_frame_->CallFunctionEvenIfScriptDisabled(
          function, global, argc, argv)));
}

}  // namespace extensions
