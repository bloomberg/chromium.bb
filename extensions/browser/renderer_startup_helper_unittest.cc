// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/renderer_startup_helper.h"

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/mock_render_process_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_messages.h"

namespace extensions {

class RendererStartupHelperTest : public ExtensionsTest {
 public:
  RendererStartupHelperTest() {}
  ~RendererStartupHelperTest() override {}

  void SetUp() override {
    ExtensionsTest::SetUp();
    helper_ = base::MakeUnique<RendererStartupHelper>(browser_context());
    registry_ =
        ExtensionRegistryFactory::GetForBrowserContext(browser_context());
    render_process_host_ =
        base::MakeUnique<content::MockRenderProcessHost>(browser_context());
    extension_ = CreateExtension("ext_1");
  }

  void TearDown() override {
    render_process_host_.reset();
    helper_.reset();
    ExtensionsTest::TearDown();
  }

 protected:
  void SimulateRenderProcessCreated(content::RenderProcessHost* rph) {
    content::NotificationService::current()->Notify(
        content::NOTIFICATION_RENDERER_PROCESS_CREATED,
        content::Source<content::RenderProcessHost>(rph),
        content::NotificationService::NoDetails());
  }

  void SimulateRenderProcessTerminated(content::RenderProcessHost* rph) {
    content::NotificationService::current()->Notify(
        content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
        content::Source<content::RenderProcessHost>(rph),
        content::NotificationService::NoDetails());
  }

  scoped_refptr<Extension> CreateExtension(const std::string& extension_id) {
    std::unique_ptr<base::DictionaryValue> manifest =
        DictionaryBuilder()
            .Set("name", "extension")
            .Set("description", "an extension")
            .Set("manifest_version", 2)
            .Set("version", "0.1")
            .Build();
    return ExtensionBuilder()
        .SetManifest(std::move(manifest))
        .SetID(extension_id)
        .Build();
  }

  scoped_refptr<Extension> CreateTheme(const std::string& extension_id) {
    std::unique_ptr<base::DictionaryValue> manifest =
        DictionaryBuilder()
            .Set("name", "theme")
            .Set("description", "a theme")
            .Set("theme", DictionaryBuilder().Build())
            .Set("manifest_version", 2)
            .Set("version", "0.1")
            .Build();
    return ExtensionBuilder()
        .SetManifest(std::move(manifest))
        .SetID(extension_id)
        .Build();
  }

  void AddExtensionToRegistry(scoped_refptr<Extension> extension) {
    registry_->AddEnabled(extension);
  }

  void RemoveExtensionFromRegistry(scoped_refptr<Extension> extension) {
    registry_->RemoveEnabled(extension->id());
  }

  bool IsProcessInitialized(content::RenderProcessHost* rph) {
    return base::ContainsKey(helper_->initialized_processes_, rph);
  }

  bool IsExtensionLoaded(const Extension& extension) {
    return base::ContainsKey(helper_->extension_process_map_, extension.id());
  }

  bool IsExtensionLoadedInProcess(const Extension& extension,
                                  content::RenderProcessHost* rph) {
    return IsExtensionLoaded(extension) &&
           base::ContainsKey(helper_->extension_process_map_[extension.id()],
                             rph);
  }

  bool IsExtensionPendingActivationInProcess(const Extension& extension,
                                             content::RenderProcessHost* rph) {
    return base::ContainsKey(helper_->pending_active_extensions_, rph) &&
           base::ContainsKey(helper_->pending_active_extensions_[rph],
                             extension.id());
  }

  std::unique_ptr<RendererStartupHelper> helper_;
  ExtensionRegistry* registry_;  // Weak.
  std::unique_ptr<content::MockRenderProcessHost> render_process_host_;
  scoped_refptr<Extension> extension_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RendererStartupHelperTest);
};

// Tests extension loading, unloading and activation and render process creation
// and termination.
TEST_F(RendererStartupHelperTest, NormalExtensionLifecycle) {
  // Initialize render process.
  EXPECT_FALSE(IsProcessInitialized(render_process_host_.get()));
  SimulateRenderProcessCreated(render_process_host_.get());
  EXPECT_TRUE(IsProcessInitialized(render_process_host_.get()));

  IPC::TestSink& sink = render_process_host_->sink();

  // Enable extension.
  sink.ClearMessages();
  EXPECT_FALSE(IsExtensionLoaded(*extension_));
  AddExtensionToRegistry(extension_);
  helper_->OnExtensionLoaded(*extension_);
  EXPECT_TRUE(
      IsExtensionLoadedInProcess(*extension_, render_process_host_.get()));
  EXPECT_FALSE(IsExtensionPendingActivationInProcess(
      *extension_, render_process_host_.get()));
  ASSERT_EQ(1u, sink.message_count());
  EXPECT_EQ(ExtensionMsg_Loaded::ID, sink.GetMessageAt(0)->type());

  // Activate extension.
  sink.ClearMessages();
  helper_->ActivateExtensionInProcess(*extension_, render_process_host_.get());
  EXPECT_FALSE(IsExtensionPendingActivationInProcess(
      *extension_, render_process_host_.get()));
  ASSERT_EQ(1u, sink.message_count());
  EXPECT_EQ(ExtensionMsg_ActivateExtension::ID, sink.GetMessageAt(0)->type());

  // Disable extension.
  sink.ClearMessages();
  RemoveExtensionFromRegistry(extension_);
  helper_->OnExtensionUnloaded(*extension_);
  EXPECT_FALSE(IsExtensionLoaded(*extension_));
  ASSERT_EQ(1u, sink.message_count());
  EXPECT_EQ(ExtensionMsg_Unloaded::ID, sink.GetMessageAt(0)->type());

  // Extension enabled again.
  sink.ClearMessages();
  AddExtensionToRegistry(extension_);
  helper_->OnExtensionLoaded(*extension_);
  EXPECT_TRUE(
      IsExtensionLoadedInProcess(*extension_, render_process_host_.get()));
  EXPECT_FALSE(IsExtensionPendingActivationInProcess(
      *extension_, render_process_host_.get()));
  ASSERT_EQ(1u, sink.message_count());
  EXPECT_EQ(ExtensionMsg_Loaded::ID, sink.GetMessageAt(0)->type());

  // Render Process terminated.
  SimulateRenderProcessTerminated(render_process_host_.get());
  EXPECT_FALSE(IsProcessInitialized(render_process_host_.get()));
  EXPECT_TRUE(IsExtensionLoaded(*extension_));
  EXPECT_FALSE(
      IsExtensionLoadedInProcess(*extension_, render_process_host_.get()));
  EXPECT_FALSE(IsExtensionPendingActivationInProcess(
      *extension_, render_process_host_.get()));
}

// Tests that activating an extension in an uninitialized render process works
// fine.
TEST_F(RendererStartupHelperTest, EnabledBeforeProcessInitialized) {
  EXPECT_FALSE(IsProcessInitialized(render_process_host_.get()));
  IPC::TestSink& sink = render_process_host_->sink();

  // Enable extension. The render process isn't initialized yet, so the
  // extension should be added to the list of extensions awaiting activation.
  sink.ClearMessages();
  AddExtensionToRegistry(extension_);
  helper_->OnExtensionLoaded(*extension_);
  helper_->ActivateExtensionInProcess(*extension_, render_process_host_.get());
  EXPECT_EQ(0u, sink.message_count());
  EXPECT_TRUE(IsExtensionLoaded(*extension_));
  EXPECT_FALSE(
      IsExtensionLoadedInProcess(*extension_, render_process_host_.get()));
  EXPECT_TRUE(IsExtensionPendingActivationInProcess(
      *extension_, render_process_host_.get()));

  // Initialize the render process.
  SimulateRenderProcessCreated(render_process_host_.get());
  // The renderer would have been sent multiple initialization messages
  // including the loading and activation messages for the extension.
  EXPECT_LE(2u, sink.message_count());
  EXPECT_TRUE(IsProcessInitialized(render_process_host_.get()));
  EXPECT_TRUE(
      IsExtensionLoadedInProcess(*extension_, render_process_host_.get()));
  EXPECT_FALSE(IsExtensionPendingActivationInProcess(
      *extension_, render_process_host_.get()));
}

// Tests that themes aren't loaded in a renderer process.
TEST_F(RendererStartupHelperTest, LoadTheme) {
  // Initialize render process.
  EXPECT_FALSE(IsProcessInitialized(render_process_host_.get()));
  SimulateRenderProcessCreated(render_process_host_.get());
  EXPECT_TRUE(IsProcessInitialized(render_process_host_.get()));

  scoped_refptr<Extension> extension(CreateTheme("theme"));
  ASSERT_TRUE(extension->is_theme());

  IPC::TestSink& sink = render_process_host_->sink();

  // Enable the theme. Verify it isn't loaded and activated in the renderer.
  sink.ClearMessages();
  EXPECT_FALSE(IsExtensionLoaded(*extension));
  AddExtensionToRegistry(extension_);
  helper_->OnExtensionLoaded(*extension);
  EXPECT_EQ(0u, sink.message_count());
  EXPECT_TRUE(IsExtensionLoaded(*extension));
  EXPECT_FALSE(
      IsExtensionLoadedInProcess(*extension, render_process_host_.get()));

  helper_->ActivateExtensionInProcess(*extension, render_process_host_.get());
  EXPECT_EQ(0u, sink.message_count());
  EXPECT_FALSE(IsExtensionPendingActivationInProcess(
      *extension, render_process_host_.get()));

  helper_->OnExtensionUnloaded(*extension);
  EXPECT_EQ(0u, sink.message_count());
  EXPECT_FALSE(IsExtensionLoaded(*extension));
}

}  // namespace extensions
