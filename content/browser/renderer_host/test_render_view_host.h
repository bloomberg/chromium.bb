// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_TEST_RENDER_VIEW_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_TEST_RENDER_VIEW_HOST_H_
#pragma once

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/message_loop.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/mock_render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/public/common/page_transition_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class BrowserContext;
class RenderProcessHostFactory;
}

namespace gfx {
class Rect;
}

class NavigationController;
class SiteInstance;
class TestTabContents;
struct ViewHostMsg_FrameNavigate_Params;

// Utility function to initialize ViewHostMsg_NavigateParams_Params
// with given |page_id|, |url| and |transition_type|.
void InitNavigateParams(ViewHostMsg_FrameNavigate_Params* params,
                        int page_id,
                        const GURL& url,
                        content::PageTransition transition_type);

// This file provides a testing framework for mocking out the RenderProcessHost
// layer. It allows you to test RenderViewHost, TabContents,
// NavigationController, and other layers above that without running an actual
// renderer process.
//
// To use, derive your test base class from RenderViewHostTestHarness.

// TestRenderViewHostView ------------------------------------------------------

// Subclass the RenderViewHost's view so that we can call Show(), etc.,
// without having side-effects.
class TestRenderWidgetHostView : public RenderWidgetHostView {
 public:
  explicit TestRenderWidgetHostView(RenderWidgetHost* rwh);
  virtual ~TestRenderWidgetHostView();

  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& pos) OVERRIDE {}
  virtual void InitAsFullscreen(
      RenderWidgetHostView* reference_host_view) OVERRIDE {}
  virtual RenderWidgetHost* GetRenderWidgetHost() const OVERRIDE;
  virtual void DidBecomeSelected() OVERRIDE {}
  virtual void WasHidden() OVERRIDE {}
  virtual void SetSize(const gfx::Size& size) OVERRIDE {}
  virtual void SetBounds(const gfx::Rect& rect) OVERRIDE {}
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeViewId GetNativeViewId() const OVERRIDE;
  virtual void MovePluginWindows(
      const std::vector<webkit::npapi::WebPluginGeometry>& moves) OVERRIDE {}
  virtual void Focus() OVERRIDE {}
  virtual void Blur() OVERRIDE {}
  virtual bool HasFocus() const OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual bool IsShowing() OVERRIDE;
  virtual gfx::Rect GetViewBounds() const OVERRIDE;
  virtual void SetIsLoading(bool is_loading) OVERRIDE {}
  virtual void UpdateCursor(const WebCursor& cursor) OVERRIDE {}
  virtual void TextInputStateChanged(ui::TextInputType state,
                                     bool can_compose_inline) OVERRIDE {}
  virtual void ImeCancelComposition() OVERRIDE {}
  virtual void DidUpdateBackingStore(
      const gfx::Rect& scroll_rect, int scroll_dx, int scroll_dy,
      const std::vector<gfx::Rect>& rects) OVERRIDE {}
  virtual void RenderViewGone(base::TerminationStatus status,
                              int error_code) OVERRIDE;
  virtual void WillDestroyRenderWidget(RenderWidgetHost* rwh) { }
  virtual void Destroy() OVERRIDE {}
  virtual void SetTooltipText(const string16& tooltip_text) OVERRIDE {}
  virtual BackingStore* AllocBackingStore(const gfx::Size& size) OVERRIDE;
  virtual void OnAcceleratedCompositingStateChange() OVERRIDE;
  virtual void AcceleratedSurfaceBuffersSwapped(
      const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params,
      int gpu_host_id) OVERRIDE;
  virtual void AcceleratedSurfacePostSubBuffer(
      const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params,
      int gpu_host_id) OVERRIDE;
#if defined(OS_MACOSX)
  virtual void SetTakesFocusOnlyOnMouseDown(bool flag) OVERRIDE {}
  virtual gfx::Rect GetViewCocoaBounds() const OVERRIDE;
  virtual void SetActive(bool active) OVERRIDE;
  virtual void SetWindowVisibility(bool visible) OVERRIDE {}
  virtual void WindowFrameChanged() OVERRIDE {}
  virtual void PluginFocusChanged(bool focused, int plugin_id) OVERRIDE;
  virtual void StartPluginIme() OVERRIDE;
  virtual bool PostProcessEventForPluginIme(
      const NativeWebKeyboardEvent& event) OVERRIDE;
  virtual gfx::PluginWindowHandle AllocateFakePluginWindowHandle(
      bool opaque,
      bool root) OVERRIDE;
  virtual void DestroyFakePluginWindowHandle(
      gfx::PluginWindowHandle window) OVERRIDE;
  virtual void AcceleratedSurfaceSetIOSurface(gfx::PluginWindowHandle window,
                                              int32 width,
                                              int32 height,
                                              uint64 surface_id) OVERRIDE;
  virtual void AcceleratedSurfaceSetTransportDIB(
      gfx::PluginWindowHandle window,
      int32 width,
      int32 height,
      TransportDIB::Handle transport_dib) OVERRIDE;
#elif defined(OS_WIN) && !defined(USE_AURA)
  virtual void WillWmDestroy() OVERRIDE;
#endif
#if defined(OS_POSIX) || defined(USE_AURA)
  virtual void GetScreenInfo(WebKit::WebScreenInfo* results) OVERRIDE {}
  virtual gfx::Rect GetRootWindowBounds() OVERRIDE;
#endif
  virtual void SetVisuallyDeemphasized(const SkColor* color,
                                       bool animate) OVERRIDE { }
  virtual void UnhandledWheelEvent(
      const WebKit::WebMouseWheelEvent& event) OVERRIDE { }
  virtual void SetHasHorizontalScrollbar(
      bool has_horizontal_scrollbar) OVERRIDE { }
  virtual void SetScrollOffsetPinning(
      bool is_pinned_to_left, bool is_pinned_to_right) OVERRIDE { }

#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
  virtual void AcceleratedSurfaceNew(
      int32 width, int32 height, uint64* surface_id,
      TransportDIB::Handle* surface_handle) { }
  virtual void AcceleratedSurfaceRelease(uint64 surface_id) { }
#endif

#if defined(TOOLKIT_USES_GTK)
  virtual void CreatePluginContainer(gfx::PluginWindowHandle id) { }
  virtual void DestroyPluginContainer(gfx::PluginWindowHandle id) { }
#endif

  virtual gfx::PluginWindowHandle GetCompositingSurface() OVERRIDE;

  virtual bool LockMouse() OVERRIDE;
  virtual void UnlockMouse() OVERRIDE;

  bool is_showing() const { return is_showing_; }

 private:
  RenderWidgetHost* rwh_;
  bool is_showing_;
};

// TestRenderViewHost ----------------------------------------------------------

// TODO(brettw) this should use a TestTabContents which should be generalized
// from the TabContents test. We will probably also need that class' version of
// CreateRenderViewForRenderManager when more complicate tests start using this.
class TestRenderViewHost : public RenderViewHost {
 public:
  TestRenderViewHost(SiteInstance* instance,
                     RenderViewHostDelegate* delegate,
                     int routing_id);
  virtual ~TestRenderViewHost();

  // Testing functions ---------------------------------------------------------

  // Calls the RenderViewHosts' private OnMessageReceived function with the
  // given message.
  bool TestOnMessageReceived(const IPC::Message& msg);

  // Calls OnMsgNavigate on the RenderViewHost with the given information,
  // setting the rest of the parameters in the message to the "typical" values.
  // This is a helper function for simulating the most common types of loads.
  void SendNavigate(int page_id, const GURL& url);

  // Calls OnMsgNavigate on the RenderViewHost with the given information,
  // including a custom content::PageTransition.  Sets the rest of the
  // parameters in the message to the "typical" values. This is a helper
  // function for simulating the most common types of loads.
  void SendNavigateWithTransition(int page_id, const GURL& url,
                                  content::PageTransition transition);

  // Calls OnMsgShouldCloseACK on the RenderViewHost with the given parameter.
  void SendShouldCloseACK(bool proceed);

  void TestOnMsgStartDragging(const WebDropData& drop_data);

  // If set, *delete_counter is incremented when this object destructs.
  void set_delete_counter(int* delete_counter) {
    delete_counter_ = delete_counter;
  }

  // Sets whether the RenderView currently exists or not. This controls the
  // return value from IsRenderViewLive, which the rest of the system uses to
  // check whether the RenderView has crashed or not.
  void set_render_view_created(bool created) {
    render_view_created_ = created;
  }

  // Returns whether the RenderViewHost is currently waiting to hear the result
  // of a before unload handler from the renderer.
  bool is_waiting_for_beforeunload_ack() const {
    return is_waiting_for_beforeunload_ack_;
  }

  // Sets whether the RenderViewHost is currently swapped out, and thus
  // filtering messages from the renderer.
  void set_is_swapped_out(bool is_swapped_out) {
    is_swapped_out_ = is_swapped_out;
  }

  // If set, navigations will appear to have loaded through a proxy
  // (ViewHostMsg_FrameNavigte_Params::was_fetched_via_proxy).
  // False by default.
  void set_simulate_fetch_via_proxy(bool proxy);

  // If set, future loads will have |mime_type| set as the mime type.
  // If not set, the mime type will default to "text/html".
  void set_contents_mime_type(const std::string& mime_type);

  // RenderViewHost overrides --------------------------------------------------

  virtual bool CreateRenderView(const string16& frame_name) OVERRIDE;
  virtual bool IsRenderViewLive() const OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(RenderViewHostTest, FilterNavigate);

  // Tracks if the caller thinks if it created the RenderView. This is so we can
  // respond to IsRenderViewLive appropriately.
  bool render_view_created_;

  // See set_delete_counter() above. May be NULL.
  int* delete_counter_;

  // See set_simulate_fetch_via_proxy() above.
  bool simulate_fetch_via_proxy_;

  // See set_contents_mime_type() above.
  std::string contents_mime_type_;

  DISALLOW_COPY_AND_ASSIGN(TestRenderViewHost);
};

// TestRenderViewHostFactory ---------------------------------------------------

// Manages creation of the RenderViewHosts using our special subclass. This
// automatically registers itself when it goes in scope, and unregisters itself
// when it goes out of scope. Since you can't have more than one factory
// registered at a time, you can only have one of these objects at a time.
class TestRenderViewHostFactory : public RenderViewHostFactory {
 public:
  explicit TestRenderViewHostFactory(
      content::RenderProcessHostFactory* rph_factory);
  virtual ~TestRenderViewHostFactory();

  virtual void set_render_process_host_factory(
      content::RenderProcessHostFactory* rph_factory);
  virtual RenderViewHost* CreateRenderViewHost(
      SiteInstance* instance,
      RenderViewHostDelegate* delegate,
      int routing_id,
      SessionStorageNamespace* session_storage) OVERRIDE;

 private:
  // This is a bit of a hack. With the current design of the site instances /
  // browsing instances, it's difficult to pass a RenderProcessHostFactory
  // around properly.
  //
  // Instead, we set it right before we create a new RenderViewHost, which
  // happens before the RenderProcessHost is created. This way, the instance
  // has the correct factory and creates our special RenderProcessHosts.
  content::RenderProcessHostFactory* render_process_host_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestRenderViewHostFactory);
};

// RenderViewHostTestHarness ---------------------------------------------------

class RenderViewHostTestHarness : public testing::Test {
 public:
  RenderViewHostTestHarness();
  virtual ~RenderViewHostTestHarness();

  NavigationController& controller();
  virtual TestTabContents* contents();
  TestRenderViewHost* rvh();
  TestRenderViewHost* pending_rvh();
  TestRenderViewHost* active_rvh();
  content::BrowserContext* browser_context();
  MockRenderProcessHost* process();

  // Frees the current tab contents for tests that want to test destruction.
  void DeleteContents();

  // Sets the current tab contents for tests that want to alter it. Takes
  // ownership of the TestTabContents passed.
  virtual void SetContents(TestTabContents* contents);

  // Creates a new TestTabContents. Ownership passes to the caller.
  TestTabContents* CreateTestTabContents();

  // Cover for |contents()->NavigateAndCommit(url)|. See
  // TestTabContents::NavigateAndCommit for details.
  void NavigateAndCommit(const GURL& url);

  // Simulates a reload of the current page.
  void Reload();

 protected:
  // testing::Test
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // This browser context will be created in SetUp if it has not already been
  // created.  This allows tests to override the browser context if they so
  // choose in their own SetUp function before calling the base class's (us)
  // SetUp().
  scoped_ptr<content::BrowserContext> browser_context_;

  MessageLoopForUI message_loop_;

  MockRenderProcessHostFactory rph_factory_;
  TestRenderViewHostFactory rvh_factory_;

 private:
  scoped_ptr<TestTabContents> contents_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostTestHarness);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_TEST_RENDER_VIEW_HOST_H_
