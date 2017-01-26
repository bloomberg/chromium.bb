// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/shell_content_renderer_client.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "components/cdm/renderer/external_clear_key_key_system_properties.h"
#include "components/web_cache/renderer/web_cache_impl.h"
#include "content/public/test/test_service.mojom.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/renderer/shell_render_view_observer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "ppapi/features/features.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "third_party/WebKit/public/web/WebTestingSupport.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "ppapi/shared_impl/ppapi_switches.h"  // nogncheck
#endif

#if defined(ENABLE_MOJO_CDM)
#include "base/feature_list.h"
#include "media/base/media_switches.h"
#endif

namespace content {

namespace {

// A test service which can be driven by browser tests for various reasons.
class TestServiceImpl : public mojom::TestService {
 public:
  explicit TestServiceImpl(mojom::TestServiceRequest request)
      : binding_(this, std::move(request)) {
    binding_.set_connection_error_handler(
        base::Bind(&TestServiceImpl::OnConnectionError,
                   base::Unretained(this)));
  }

  ~TestServiceImpl() override {}

 private:
  void OnConnectionError() { delete this; }

  // mojom::TestService:
  void DoSomething(const DoSomethingCallback& callback) override {
    // Instead of responding normally, unbind the pipe, write some garbage,
    // and go away.
    const std::string kBadMessage = "This is definitely not a valid response!";
    mojo::ScopedMessagePipeHandle pipe = binding_.Unbind().PassMessagePipe();
    MojoResult rv = mojo::WriteMessageRaw(
        pipe.get(), kBadMessage.data(), kBadMessage.size(), nullptr, 0,
        MOJO_WRITE_MESSAGE_FLAG_NONE);
    DCHECK_EQ(rv, MOJO_RESULT_OK);

    // Deletes this.
    OnConnectionError();
  }

  void DoTerminateProcess(const DoTerminateProcessCallback& callback) override {
    NOTREACHED();
  }

  void CreateFolder(const CreateFolderCallback& callback) override {
    NOTREACHED();
  }

  void GetRequestorName(const GetRequestorNameCallback& callback) override {
    callback.Run("Not implemented.");
  }

  void CreateSharedBuffer(const std::string& message,
                          const CreateSharedBufferCallback& callback) override {
    NOTREACHED();
  }

  mojo::Binding<mojom::TestService> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestServiceImpl);
};

void CreateTestService(mojom::TestServiceRequest request) {
  // Owns itself.
  new TestServiceImpl(std::move(request));
}

}  // namespace

ShellContentRendererClient::ShellContentRendererClient() {}

ShellContentRendererClient::~ShellContentRendererClient() {
}

void ShellContentRendererClient::RenderThreadStarted() {
  web_cache_impl_.reset(new web_cache::WebCacheImpl());
}

void ShellContentRendererClient::RenderViewCreated(RenderView* render_view) {
  new ShellRenderViewObserver(render_view);
}

bool ShellContentRendererClient::IsPluginAllowedToUseCompositorAPI(
    const GURL& url) {
#if BUILDFLAG(ENABLE_PLUGINS)
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnablePepperTesting);
#else
  return false;
#endif
}

bool ShellContentRendererClient::IsPluginAllowedToUseDevChannelAPIs() {
#if BUILDFLAG(ENABLE_PLUGINS)
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnablePepperTesting);
#else
  return false;
#endif
}

void ShellContentRendererClient::DidInitializeWorkerContextOnWorkerThread(
    v8::Local<v8::Context> context) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kExposeInternalsForTesting)) {
    blink::WebTestingSupport::injectInternalsObject(context);
  }
}

void ShellContentRendererClient::ExposeInterfacesToBrowser(
    service_manager::InterfaceRegistry* interface_registry) {
  interface_registry->AddInterface<mojom::TestService>(
      base::Bind(&CreateTestService));
}

#if defined(ENABLE_MOJO_CDM)
void ShellContentRendererClient::AddSupportedKeySystems(
    std::vector<std::unique_ptr<media::KeySystemProperties>>* key_systems) {
  if (!base::FeatureList::IsEnabled(media::kExternalClearKeyForTesting))
    return;

  static const char kExternalClearKeyKeySystem[] =
      "org.chromium.externalclearkey";
  key_systems->emplace_back(
      new cdm::ExternalClearKeyProperties(kExternalClearKeyKeySystem));
}
#endif

}  // namespace content
