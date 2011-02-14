// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_TEST_TEST_RENDER_VIEW_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_TEST_TEST_RENDER_VIEW_HOST_H_
#pragma once

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/message_loop.h"
#include "build/build_config.h"
#include "chrome/browser/renderer_host/mock_render_process_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_factory.h"
#include "chrome/common/page_transition_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {
class Rect;
}

class NavigationController;
class SiteInstance;
class TestingProfile;
class TestTabContents;
struct WebMenuItem;
struct ViewHostMsg_FrameNavigate_Params;

// Utility function to initialize ViewHostMsg_NavigateParams_Params
// with given |page_id|, |url| and |transition_type|.
void InitNavigateParams(ViewHostMsg_FrameNavigate_Params* params,
                        int page_id,
                        const GURL& url,
                        PageTransition::Type transition_type);

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
                           const gfx::Rect& pos) {}
  virtual void InitAsFullscreen() {}
  virtual RenderWidgetHost* GetRenderWidgetHost() const;
  virtual void DidBecomeSelected() {}
  virtual void WasHidden() {}
  virtual void SetSize(const gfx::Size& size) {}
  virtual gfx::NativeView GetNativeView();
  virtual void MovePluginWindows(
      const std::vector<webkit::npapi::WebPluginGeometry>& moves) {}
#if defined(OS_WIN)
  virtual void ForwardMouseEventToRenderer(UINT message,
                                           WPARAM wparam,
                                           LPARAM lparam) {}
#endif
  virtual void Focus() {}
  virtual void Blur() {}
  virtual bool HasFocus();
  virtual void AdvanceFocus(bool reverse) {}
  virtual void Show();
  virtual void Hide();
  virtual bool IsShowing();
  virtual gfx::Rect GetViewBounds() const;
  virtual void SetIsLoading(bool is_loading) {}
  virtual void UpdateCursor(const WebCursor& cursor) {}
  virtual void UpdateCursorIfOverSelf() {}
  virtual void ImeUpdateTextInputState(WebKit::WebTextInputType state,
                                       const gfx::Rect& caret_rect) {}
  virtual void ImeCancelComposition() {}
  virtual void DidUpdateBackingStore(
      const gfx::Rect& scroll_rect, int scroll_dx, int scroll_dy,
      const std::vector<gfx::Rect>& rects) {}
  virtual void RenderViewGone(base::TerminationStatus status,
                              int error_code);
  virtual void WillDestroyRenderWidget(RenderWidgetHost* rwh) { }
  virtual void Destroy() {}
  virtual void PrepareToDestroy() {}
  virtual void SetTooltipText(const std::wstring& tooltip_text) {}
  virtual BackingStore* AllocBackingStore(const gfx::Size& size);
#if defined(OS_MACOSX)
  virtual void SetTakesFocusOnlyOnMouseDown(bool flag) {}
  virtual void ShowPopupWithItems(gfx::Rect bounds,
                                  int item_height,
                                  double item_font_size,
                                  int selected_item,
                                  const std::vector<WebMenuItem>& items,
                                  bool right_aligned);
  virtual gfx::Rect GetViewCocoaBounds() const;
  virtual gfx::Rect GetRootWindowRect();
  virtual void SetActive(bool active);
  virtual void SetWindowVisibility(bool visible) {}
  virtual void WindowFrameChanged() {}
  virtual void PluginFocusChanged(bool focused, int plugin_id);
  virtual void StartPluginIme();
  virtual bool PostProcessEventForPluginIme(
      const NativeWebKeyboardEvent& event);
  virtual gfx::PluginWindowHandle AllocateFakePluginWindowHandle(
      bool opaque,
      bool root);
  virtual void DestroyFakePluginWindowHandle(gfx::PluginWindowHandle window);
  virtual void AcceleratedSurfaceSetIOSurface(gfx::PluginWindowHandle window,
                                              int32 width,
                                              int32 height,
                                              uint64 surface_id);
  virtual void AcceleratedSurfaceSetTransportDIB(
      gfx::PluginWindowHandle window,
      int32 width,
      int32 height,
      TransportDIB::Handle transport_dib);
  virtual void AcceleratedSurfaceBuffersSwapped(
      gfx::PluginWindowHandle window,
      uint64 surface_id,
      int renderer_id,
      int32 route_id,
      uint64 swap_buffers_count);
  virtual void GpuRenderingStateDidChange();
#elif defined(OS_WIN)
  virtual gfx::PluginWindowHandle GetCompositorHostWindow();
  virtual void WillWmDestroy();
  virtual void ShowCompositorHostWindow(bool show);
#endif
  virtual void SetVisuallyDeemphasized(const SkColor* color, bool animate) { }

#if defined(TOOLKIT_USES_GTK)
  virtual void CreatePluginContainer(gfx::PluginWindowHandle id) { }
  virtual void DestroyPluginContainer(gfx::PluginWindowHandle id) { }
  virtual void AcceleratedCompositingActivated(bool activated) { }
#endif

  virtual bool ContainsNativeView(gfx::NativeView native_view) const;

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
  // including a custom PageTransition::Type.  Sets the rest of the parameters
  // in the message to the "typical" values.
  // This is a helper function for simulating the most common types of loads.
  void SendNavigateWithTransition(int page_id, const GURL& url,
                                  PageTransition::Type transition);

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

  // RenderViewHost overrides --------------------------------------------------

  virtual bool CreateRenderView(const string16& frame_name);
  virtual bool IsRenderViewLive() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(RenderViewHostTest, FilterNavigate);

  // Tracks if the caller thinks if it created the RenderView. This is so we can
  // respond to IsRenderViewLive appropriately.
  bool render_view_created_;

  // See set_delete_counter() above. May be NULL.
  int* delete_counter_;

  DISALLOW_COPY_AND_ASSIGN(TestRenderViewHost);
};

// TestRenderViewHostFactory ---------------------------------------------------

// Manages creation of the RenderViewHosts using our special subclass. This
// automatically registers itself when it goes in scope, and unregisters itself
// when it goes out of scope. Since you can't have more than one factory
// registered at a time, you can only have one of these objects at a time.
class TestRenderViewHostFactory : public RenderViewHostFactory {
 public:
  explicit TestRenderViewHostFactory(RenderProcessHostFactory* rph_factory);
  virtual ~TestRenderViewHostFactory();

  virtual void set_render_process_host_factory(
      RenderProcessHostFactory* rph_factory);
  virtual RenderViewHost* CreateRenderViewHost(
      SiteInstance* instance,
      RenderViewHostDelegate* delegate,
      int routing_id,
      SessionStorageNamespace* session_storage);

 private:
  // This is a bit of a hack. With the current design of the site instances /
  // browsing instances, it's difficult to pass a RenderProcessHostFactory
  // around properly.
  //
  // Instead, we set it right before we create a new RenderViewHost, which
  // happens before the RenderProcessHost is created. This way, the instance
  // has the correct factory and creates our special RenderProcessHosts.
  RenderProcessHostFactory* render_process_host_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestRenderViewHostFactory);
};

// RenderViewHostTestHarness ---------------------------------------------------

class RenderViewHostTestHarness : public testing::Test {
 public:
  RenderViewHostTestHarness();
  virtual ~RenderViewHostTestHarness();

  NavigationController& controller();
  TestTabContents* contents();
  TestRenderViewHost* rvh();
  TestRenderViewHost* pending_rvh();
  TestRenderViewHost* active_rvh();
  TestingProfile* profile();
  MockRenderProcessHost* process();

  // Frees the current tab contents for tests that want to test destruction.
  void DeleteContents();

  // Creates a new TestTabContents. Ownership passes to the caller.
  TestTabContents* CreateTestTabContents();

  // Cover for |contents()->NavigateAndCommit(url)|. See
  // TestTabContents::NavigateAndCommit for details.
  void NavigateAndCommit(const GURL& url);

  // Simulates a reload of the current page.
  void Reload();

 protected:
  // testing::Test
  virtual void SetUp();
  virtual void TearDown();

  // This profile will be created in SetUp if it has not already been created.
  // This allows tests to override the profile if they so choose in their own
  // SetUp function before calling the base class's (us) SetUp().
  scoped_ptr<TestingProfile> profile_;

  MessageLoopForUI message_loop_;

  MockRenderProcessHostFactory rph_factory_;
  TestRenderViewHostFactory rvh_factory_;

  scoped_ptr<TestTabContents> contents_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostTestHarness);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_TEST_TEST_RENDER_VIEW_HOST_H_
