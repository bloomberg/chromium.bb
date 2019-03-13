// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_FRAME_IMPL_H_
#define FUCHSIA_ENGINE_BROWSER_FRAME_IMPL_H_

#include <lib/fidl/cpp/binding_set.h>
#include <lib/zx/channel.h>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/platform_shared_memory_region.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "fuchsia/engine/browser/discarding_event_filter.h"
#include "fuchsia/engine/on_load_script_injector.mojom.h"
#include "fuchsia/fidl/chromium/web/cpp/fidl.h"
#include "ui/aura/window_tree_host.h"
#include "ui/wm/core/focus_controller.h"
#include "url/gurl.h"

namespace aura {
class WindowTreeHost;
}  // namespace aura

namespace content {
class WebContents;
}  // namespace content

class ContextImpl;

// Implementation of Frame from //fuchsia/fidl/frame.fidl.
// Implements a Frame service, which is a wrapper for a WebContents instance.
class FrameImpl : public chromium::web::Frame,
                  public chromium::web::NavigationController,
                  public content::WebContentsObserver,
                  public content::WebContentsDelegate {
 public:
  FrameImpl(std::unique_ptr<content::WebContents> web_contents,
            ContextImpl* context,
            fidl::InterfaceRequest<chromium::web::Frame> frame_request);
  ~FrameImpl() override;

  zx::unowned_channel GetBindingChannelForTest() const;

  content::WebContents* web_contents_for_test() { return web_contents_.get(); }
  bool has_view_for_test() { return window_tree_host_ != nullptr; }

  // chromium::web::Frame implementation.
  void CreateView(fuchsia::ui::gfx::ExportToken view_token) override;
  void CreateView2(
      zx::eventpair view_token,
      fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> incoming_services,
      fidl::InterfaceHandle<fuchsia::sys::ServiceProvider> outgoing_services)
      override;
  void GetNavigationController(
      fidl::InterfaceRequest<chromium::web::NavigationController> controller)
      override;
  void SetJavaScriptLogLevel(chromium::web::LogLevel level) override;
  void SetNavigationEventObserver(
      fidl::InterfaceHandle<chromium::web::NavigationEventObserver> observer)
      override;
  void ExecuteJavaScript(std::vector<std::string> origins,
                         fuchsia::mem::Buffer script,
                         chromium::web::ExecuteMode mode,
                         ExecuteJavaScriptCallback callback) override;
  void PostMessage(chromium::web::WebMessage message,
                   std::string target_origin,
                   PostMessageCallback callback) override;
  void SetEnableInput(bool enable_input) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(FrameImplTest, DelayedNavigationEventAck);
  FRIEND_TEST_ALL_PREFIXES(FrameImplTest, NavigationObserverDisconnected);
  FRIEND_TEST_ALL_PREFIXES(FrameImplTest, NoNavigationObserverAttached);
  FRIEND_TEST_ALL_PREFIXES(FrameImplTest, ReloadFrame);
  FRIEND_TEST_ALL_PREFIXES(FrameImplTest, Stop);

  struct OriginScopedScript {
    OriginScopedScript(std::vector<std::string> origins,
                       base::ReadOnlySharedMemoryRegion script);
    ~OriginScopedScript();

    std::vector<std::string> origins;

    // A shared memory buffer containing the script, encoded as UTF16.
    base::ReadOnlySharedMemoryRegion script;

   private:
    DISALLOW_COPY_AND_ASSIGN(OriginScopedScript);
  };

  aura::Window* root_window() const { return window_tree_host_->window(); }

  // Release the resources associated with the View, if one is active.
  void TearDownView();

  // Sends |pending_navigation_event_| to the observer if there are any changes
  // to be reported.
  void MaybeSendNavigationEvent();

  // chromium::web::NavigationController implementation.
  void LoadUrl(std::string url, chromium::web::LoadUrlParams params) override;
  void LoadUrl2(std::string url, chromium::web::LoadUrlParams2 params) override;
  void GoBack() override;
  void GoForward() override;
  void Stop() override;
  void Reload(chromium::web::ReloadType type) override;
  void GetVisibleEntry(GetVisibleEntryCallback callback) override;

  // content::WebContentsDelegate implementation.
  bool ShouldCreateWebContents(
      content::WebContents* web_contents,
      content::RenderFrameHost* opener,
      content::SiteInstance* source_site_instance,
      int32_t route_id,
      int32_t main_frame_route_id,
      int32_t main_frame_widget_route_id,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url,
      const std::string& partition_id,
      content::SessionStorageNamespace* session_storage_namespace) override;
  bool DidAddMessageToConsole(content::WebContents* source,
                              int32_t level,
                              const base::string16& message,
                              int32_t line_no,
                              const base::string16& source_id) override;

  // content::WebContentsObserver implementation.
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;

  std::unique_ptr<aura::WindowTreeHost> window_tree_host_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<wm::FocusController> focus_controller_;

  DiscardingEventFilter discarding_event_filter_;
  chromium::web::NavigationEventObserverPtr navigation_observer_;
  chromium::web::NavigationEntry cached_navigation_state_;
  chromium::web::NavigationEvent pending_navigation_event_;
  bool waiting_for_navigation_event_ack_;
  bool pending_navigation_event_is_dirty_;
  chromium::web::LogLevel log_level_ = chromium::web::LogLevel::NONE;
  std::list<OriginScopedScript> before_load_scripts_;

  ContextImpl* context_ = nullptr;
  fidl::Binding<chromium::web::Frame> binding_;
  fidl::BindingSet<chromium::web::NavigationController> controller_bindings_;

  DISALLOW_COPY_AND_ASSIGN(FrameImpl);
};

#endif  // FUCHSIA_ENGINE_BROWSER_FRAME_IMPL_H_
