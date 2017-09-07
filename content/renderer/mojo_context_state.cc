// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mojo_context_state.h"

#include <stddef.h>

#include <map>
#include <string>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/stl_util.h"
#include "content/grit/content_resources.h"
#include "content/public/child/child_url_loader_factory_getter.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/resource_fetcher.h"
#include "content/renderer/mojo_bindings_controller.h"
#include "content/renderer/mojo_main_runner.h"
#include "gin/converter.h"
#include "gin/modules/module_registry.h"
#include "gin/per_context_data.h"
#include "gin/public/context_holder.h"
#include "gin/try_catch.h"
#include "mojo/public/js/constants.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"

using v8::Context;
using v8::HandleScope;
using v8::Isolate;
using v8::Object;
using v8::ObjectTemplate;
using v8::Script;

namespace content {

namespace {

void RunMain(base::WeakPtr<gin::Runner> runner,
             v8::Local<v8::Value> module) {
  v8::Isolate* isolate = runner->GetContextHolder()->isolate();
  v8::Local<v8::Function> start;
  CHECK(gin::ConvertFromV8(isolate, module, &start));
  runner->Call(start, runner->global(), 0, NULL);
}

using ModuleSourceMap =
    std::map<std::string, scoped_refptr<base::RefCountedMemory>>;

base::LazyInstance<std::unique_ptr<ModuleSourceMap>>::Leaky g_module_sources;

scoped_refptr<base::RefCountedMemory> GetBuiltinModuleData(
    const std::string& path) {
  static const struct {
    const char* path;
    const int id;
  } kBuiltinModuleResources[] = {
      {mojo::kAssociatedBindingsModuleName, IDR_MOJO_ASSOCIATED_BINDINGS_JS},
      {mojo::kBindingsModuleName, IDR_MOJO_BINDINGS_JS},
      {mojo::kBufferModuleName, IDR_MOJO_BUFFER_JS},
      {mojo::kCodecModuleName, IDR_MOJO_CODEC_JS},
      {mojo::kConnectorModuleName, IDR_MOJO_CONNECTOR_JS},
      {mojo::kControlMessageHandlerModuleName,
       IDR_MOJO_CONTROL_MESSAGE_HANDLER_JS},
      {mojo::kControlMessageProxyModuleName, IDR_MOJO_CONTROL_MESSAGE_PROXY_JS},
      {mojo::kInterfaceControlMessagesMojom,
       IDR_MOJO_INTERFACE_CONTROL_MESSAGES_MOJOM_JS},
      {mojo::kInterfaceEndpointClientModuleName,
       IDR_MOJO_INTERFACE_ENDPOINT_CLIENT_JS},
      {mojo::kInterfaceEndpointHandleModuleName,
       IDR_MOJO_INTERFACE_ENDPOINT_HANDLE_JS},
      {mojo::kInterfaceTypesModuleName, IDR_MOJO_INTERFACE_TYPES_JS},
      {mojo::kPipeControlMessageHandlerModuleName,
       IDR_MOJO_PIPE_CONTROL_MESSAGE_HANDLER_JS},
      {mojo::kPipeControlMessageProxyModuleName,
       IDR_MOJO_PIPE_CONTROL_MESSAGE_PROXY_JS},
      {mojo::kPipeControlMessagesMojom,
       IDR_MOJO_PIPE_CONTROL_MESSAGES_MOJOM_JS},
      {mojo::kRouterModuleName, IDR_MOJO_ROUTER_JS},
      {mojo::kUnicodeModuleName, IDR_MOJO_UNICODE_JS},
      {mojo::kValidatorModuleName, IDR_MOJO_VALIDATOR_JS},
  };

  std::unique_ptr<ModuleSourceMap>& module_sources = g_module_sources.Get();
  if (!module_sources) {
    // Initialize the module source map on first access.
    module_sources.reset(new ModuleSourceMap);
    for (size_t i = 0; i < arraysize(kBuiltinModuleResources); ++i) {
      const auto& resource = kBuiltinModuleResources[i];
      scoped_refptr<base::RefCountedMemory> data =
          GetContentClient()->GetDataResourceBytes(resource.id);
      DCHECK_GT(data->size(), 0u);
      module_sources->insert(std::make_pair(std::string(resource.path), data));
    }
  }

  DCHECK(module_sources);
  auto source_iter = module_sources->find(path);
  if (source_iter == module_sources->end())
    return nullptr;
  return source_iter->second;
}

std::string GetModulePrefixForBindingsType(MojoBindingsType bindings_type,
                                           blink::WebLocalFrame* frame) {
  switch (bindings_type) {
    case MojoBindingsType::FOR_WEB_UI:
      return frame->GetSecurityOrigin().ToString().Utf8() + "/";
    case MojoBindingsType::FOR_LAYOUT_TESTS:
      return "layout-test-mojom://";
  }
  NOTREACHED();
  return "";
}

}  // namespace

MojoContextState::MojoContextState(blink::WebLocalFrame* frame,
                                   v8::Local<v8::Context> context,
                                   MojoBindingsType bindings_type)
    : frame_(frame),
      module_added_(false),
      module_prefix_(GetModulePrefixForBindingsType(bindings_type, frame)) {
  gin::PerContextData* context_data = gin::PerContextData::From(context);
  gin::ContextHolder* context_holder = context_data->context_holder();
  runner_.reset(new MojoMainRunner(frame_, context_holder));
  gin::Runner::Scope scoper(runner_.get());
  gin::ModuleRegistry::From(context)->AddObserver(this);
  content::RenderFrame::FromWebFrame(frame)
      ->EnsureMojoBuiltinsAreAvailable(context_holder->isolate(), context);
  v8::Local<v8::Object> install_target;
  if (bindings_type == MojoBindingsType::FOR_LAYOUT_TESTS) {
    // In layout tests we install the module system under 'gin.define'
    // for now to avoid globally exposing something as generic as 'define'.
    //
    // TODO(rockot): Remove this if/when we can integrate gin + ES6 modules.
    install_target = v8::Object::New(context->GetIsolate());
    gin::SetProperty(context->GetIsolate(), context->Global(),
                     gin::StringToSymbol(context->GetIsolate(), "gin"),
                     install_target);
  } else {
    // Otherwise we're fine installing a global 'define'.
    install_target = context->Global();
  }
  gin::ModuleRegistry::InstallGlobals(context->GetIsolate(), install_target);
  // Warning |frame| may be destroyed.
  // TODO(sky): add test for this.
}

MojoContextState::~MojoContextState() {
  gin::Runner::Scope scoper(runner_.get());
  gin::ModuleRegistry::From(
      runner_->GetContextHolder()->context())->RemoveObserver(this);
}

void MojoContextState::Run() {
  gin::ContextHolder* context_holder = runner_->GetContextHolder();
  gin::ModuleRegistry::From(context_holder->context())->LoadModule(
      context_holder->isolate(),
      "main",
      base::Bind(RunMain, runner_->GetWeakPtr()));
}

void MojoContextState::FetchModules(const std::vector<std::string>& ids) {
  gin::Runner::Scope scoper(runner_.get());
  gin::ContextHolder* context_holder = runner_->GetContextHolder();
  gin::ModuleRegistry* registry = gin::ModuleRegistry::From(
      context_holder->context());
  for (size_t i = 0; i < ids.size(); ++i) {
    if (fetched_modules_.find(ids[i]) == fetched_modules_.end() &&
        registry->available_modules().count(ids[i]) == 0) {
      scoped_refptr<base::RefCountedMemory> data = GetBuiltinModuleData(ids[i]);
      if (data)
        runner_->Run(std::string(data->front_as<char>(), data->size()), ids[i]);
      else
        FetchModule(ids[i]);
    }
  }
}

void MojoContextState::FetchModule(const std::string& id) {
  static const net::NetworkTrafficAnnotationTag network_traffic_annotation_tag =
      net::DefineNetworkTrafficAnnotation("mojo_context_state", R"(
    semantics {
      sender: "MojoContextState"
      description:
        "Chrome does fetch for AMD-style module loading of Mojo JavaScript "
        "bindings."
      trigger:
        "When AMD-style module loading of Mojo JavaScript bindings is used."
      data:
        "Load JavaScript files for Mojo bindings from embedded resources. "
        "Nothing is sent over networks."
      destination: OTHER
    }
    policy {
      cookies_allowed: NO
      setting: "These requests cannot be disabled in settings."
      policy_exception_justification:
        "Not implemented. Without these requests, Chrome will not work."
    })");

  const GURL url(module_prefix_ + id);
  // TODO(sky): better error checks here?
  DCHECK(url.is_valid() && !url.is_empty());
  DCHECK(fetched_modules_.find(id) == fetched_modules_.end());
  fetched_modules_.insert(id);
  module_fetchers_.push_back(ResourceFetcher::Create(url));
  module_fetchers_.back()->Start(
      frame_, blink::WebURLRequest::kRequestContextScript,
      RenderFrame::FromWebFrame(frame_)
          ->GetDefaultURLLoaderFactoryGetter()
          ->GetNetworkLoaderFactory(),
      network_traffic_annotation_tag,
      base::BindOnce(&MojoContextState::OnFetchModuleComplete,
                     base::Unretained(this), module_fetchers_.back().get(),
                     id));
}

void MojoContextState::OnFetchModuleComplete(
    ResourceFetcher* fetcher,
    const std::string& id,
    const blink::WebURLResponse& response,
    const std::string& data) {
  if (response.IsNull()) {
    LOG(ERROR) << "Failed to fetch source for module \"" << id << "\"";
    return;
  }
  DCHECK_EQ(module_prefix_ + id, response.Url().GetString().Utf8());
  // We can't delete fetch right now as the arguments to this function come from
  // it and are used below. Instead use a scope_ptr to cleanup.
  auto iter =
      std::find_if(module_fetchers_.begin(), module_fetchers_.end(),
                   [fetcher](const std::unique_ptr<ResourceFetcher>& item) {
                     return item.get() == fetcher;
                   });
  std::unique_ptr<ResourceFetcher> deleter = std::move(*iter);
  module_fetchers_.erase(iter);

  if (data.empty()) {
    LOG(ERROR) << "Fetched empty source for module \"" << id << "\"";
    return;
  }

  runner_->Run(data, id);
}

void MojoContextState::OnDidAddPendingModule(
    const std::string& id,
    const std::vector<std::string>& dependencies) {
  FetchModules(dependencies);

  gin::ContextHolder* context_holder = runner_->GetContextHolder();
  gin::ModuleRegistry* registry = gin::ModuleRegistry::From(
      context_holder->context());
  registry->AttemptToLoadMoreModules(context_holder->isolate());
}

}  // namespace content
