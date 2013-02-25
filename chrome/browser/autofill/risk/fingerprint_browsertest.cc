// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/risk/fingerprint.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/port.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_service.h"
#include "chrome/browser/autofill/risk/proto/fingerprint.pb.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "net/base/load_states.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebScreenInfo.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/rect.h"

namespace autofill {
namespace risk {

namespace {

using content::BrowserContext;
using content::InterstitialPage;
using content::KeyboardListener;
using content::NativeWebKeyboardEvent;
using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::PageTransition;
using content::RendererPreferences;
using content::RenderProcessHost;
using content::RenderViewHost;
using content::RenderWidgetHost;
using content::RenderWidgetHostImpl;
using content::RenderWidgetHostView;
using content::Referrer;
using content::SavePageType;
using content::SessionStorageNamespace;
using content::SessionStorageNamespaceMap;
using content::SiteInstance;
using content::WebContents;
using content::WebContentsDelegate;
using content::WebContentsView;
using content::WebUI;

const int64 kGaiaId = GG_INT64_C(99194853094755497);
const char kCharset[] = "UTF-8";
const char kAcceptLanguages[] = "en-US,en";
const int kScreenColorDepth = 53;

class TestWebContentsView : public WebContentsView {
 public:
  explicit TestWebContentsView(const gfx::Rect& bounds) : bounds_(bounds) {}
  virtual ~TestWebContentsView() {}

  virtual void GetContainerBounds(gfx::Rect* out) const OVERRIDE {
    *out = bounds_;
  }

  // The rest of WebContentsView:
  virtual gfx::NativeView GetNativeView() const OVERRIDE {
    return gfx::NativeView();
  }
  virtual gfx::NativeView GetContentNativeView() const OVERRIDE {
    return gfx::NativeView();
  }
  virtual gfx::NativeWindow GetTopLevelNativeWindow() const OVERRIDE {
    return gfx::NativeWindow();
  }
  virtual void OnTabCrashed(base::TerminationStatus status,
                            int error_code) OVERRIDE {}
  virtual void SizeContents(const gfx::Size& size) OVERRIDE {}
  virtual void Focus() OVERRIDE {}
  virtual void SetInitialFocus() OVERRIDE {}
  virtual void StoreFocus() OVERRIDE {}
  virtual void RestoreFocus() OVERRIDE {}
  virtual WebDropData* GetDropData() const OVERRIDE { return NULL; }
  virtual gfx::Rect GetViewBounds() const OVERRIDE { return gfx::Rect(); }
#if defined(OS_MACOSX)
  virtual void SetAllowOverlappingViews(bool overlapping) OVERRIDE {}
#endif

 private:
  const gfx::Rect bounds_;

  DISALLOW_COPY_AND_ASSIGN(TestWebContentsView);
};

class TestRenderWidgetHost : public RenderWidgetHost {
 public:
  TestRenderWidgetHost(const gfx::Rect& screen_bounds,
                       const gfx::Rect& available_screen_bounds) {
    screen_info_.depth = kScreenColorDepth;
    screen_info_.rect = WebKit::WebRect(screen_bounds);
    screen_info_.availableRect = WebKit::WebRect(available_screen_bounds);
  }
  virtual ~TestRenderWidgetHost() {}

  virtual void GetWebScreenInfo(WebKit::WebScreenInfo* result) OVERRIDE {
    *result = screen_info_;
  }

  // The rest of RenderWidgetHost:
  virtual void Undo() OVERRIDE {}
  virtual void Redo() OVERRIDE {}
  virtual void Cut() OVERRIDE {}
  virtual void Copy() OVERRIDE {}
  virtual void CopyToFindPboard() OVERRIDE {}
  virtual void Paste() OVERRIDE {}
  virtual void PasteAndMatchStyle() OVERRIDE {}
  virtual void Delete() OVERRIDE {}
  virtual void SelectAll() OVERRIDE {}
  virtual void UpdateTextDirection(
      WebKit::WebTextDirection direction) OVERRIDE {}
  virtual void NotifyTextDirection() OVERRIDE {}
  virtual void Focus() OVERRIDE {}
  virtual void Blur() OVERRIDE {}
  virtual void SetActive(bool active) OVERRIDE {}
  virtual void CopyFromBackingStore(
      const gfx::Rect& src_rect,
      const gfx::Size& accelerated_dst_size,
      const base::Callback<void(bool, const SkBitmap&)>& callback) OVERRIDE {}
#if defined(TOOLKIT_GTK)
  virtual bool CopyFromBackingStoreToGtkWindow(const gfx::Rect& dest_rect,
                                               GdkWindow* target) OVERRIDE {
    return false;
  }
#elif defined(OS_MACOSX)
  virtual gfx::Size GetBackingStoreSize() OVERRIDE { return gfx::Size(); }
  virtual bool CopyFromBackingStoreToCGContext(const CGRect& dest_rect,
                                               CGContextRef target) OVERRIDE {
    return false;
  }
#endif
  virtual void EnableFullAccessibilityMode() OVERRIDE {}
  virtual void ForwardMouseEvent(
      const WebKit::WebMouseEvent& mouse_event) OVERRIDE {}
  virtual void ForwardWheelEvent(
      const WebKit::WebMouseWheelEvent& wheel_event) OVERRIDE {}
  virtual void ForwardKeyboardEvent(
      const NativeWebKeyboardEvent& key_event) OVERRIDE {}
  virtual const gfx::Vector2d& GetLastScrollOffset() const OVERRIDE {
    return vector_2d_;
  }
  virtual RenderProcessHost* GetProcess() const OVERRIDE { return NULL; }
  virtual int GetRoutingID() const OVERRIDE { return 0; }
  virtual RenderWidgetHostView* GetView() const OVERRIDE { return NULL; }
  virtual bool IsLoading() const OVERRIDE { return false; }
  virtual bool IsRenderView() const OVERRIDE { return false; }
  virtual void PaintAtSize(TransportDIB::Handle dib_handle,
                           int tag,
                           const gfx::Size& page_size,
                           const gfx::Size& desired_size) OVERRIDE {}
  virtual void Replace(const string16& word) OVERRIDE {}
  virtual void ReplaceMisspelling(const string16& word) OVERRIDE {}
  virtual void ResizeRectChanged(const gfx::Rect& new_rect) OVERRIDE {}
  virtual void RestartHangMonitorTimeout() OVERRIDE {}
  virtual void SetIgnoreInputEvents(bool ignore_input_events) OVERRIDE {}
  virtual void Stop() OVERRIDE {}
  virtual void WasResized() OVERRIDE {}
  virtual void AddKeyboardListener(KeyboardListener* listener) OVERRIDE {}
  virtual void RemoveKeyboardListener(KeyboardListener* listener) OVERRIDE {}
  virtual bool Send(IPC::Message* msg) OVERRIDE { return false; }
  virtual RenderWidgetHostImpl* AsRenderWidgetHostImpl() OVERRIDE {
    return NULL;
  }

 private:
  WebKit::WebScreenInfo screen_info_;

  // For const-reference return value.
  gfx::Vector2d vector_2d_;

  DISALLOW_COPY_AND_ASSIGN(TestRenderWidgetHost);
};

class TestRenderWidgetHostView : public RenderWidgetHostView {
 public:
  TestRenderWidgetHostView(const gfx::Rect& screen_bounds,
                           const gfx::Rect& available_screen_bounds)
      : host_(screen_bounds, available_screen_bounds) {}
  virtual ~TestRenderWidgetHostView() {}

  virtual RenderWidgetHost* GetRenderWidgetHost() const OVERRIDE {
    return &host_;
  }

  // The rest of RenderWidgetHostView:
  virtual void InitAsChild(gfx::NativeView parent_view) OVERRIDE {}
  virtual void SetSize(const gfx::Size& size) OVERRIDE {}
  virtual void SetBounds(const gfx::Rect& rect) OVERRIDE {}
  virtual gfx::NativeView GetNativeView() const OVERRIDE {
    return gfx::NativeView();
  }
  virtual gfx::NativeViewId GetNativeViewId() const OVERRIDE {
    return gfx::NativeViewId();
  }
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() OVERRIDE {
    return gfx::NativeViewAccessible();
  }
  virtual void Focus() OVERRIDE {}
  virtual bool HasFocus() const OVERRIDE { return false; }
  virtual bool IsSurfaceAvailableForCopy() const OVERRIDE { return false; }
  virtual void Show() OVERRIDE {}
  virtual void Hide() OVERRIDE {}
  virtual bool IsShowing() OVERRIDE { return false; }
  virtual gfx::Rect GetViewBounds() const OVERRIDE { return gfx::Rect(); }
  virtual bool IsShowingContextMenu() const OVERRIDE { return false; }
  virtual void SetShowingContextMenu(bool showing) OVERRIDE {}
  virtual string16 GetSelectedText() const OVERRIDE { return string16(); }
#if defined(OS_MACOSX)
  virtual void SetActive(bool active) OVERRIDE {}
  virtual void SetTakesFocusOnlyOnMouseDown(bool flag) OVERRIDE {}
  virtual void SetWindowVisibility(bool visible) OVERRIDE {}
  virtual void WindowFrameChanged() OVERRIDE {}
  virtual void ShowDefinitionForSelection() OVERRIDE {}
  virtual bool SupportsSpeech() const OVERRIDE { return false; }
  virtual void SpeakSelection() OVERRIDE {}
  virtual bool IsSpeaking() const OVERRIDE { return false; }
  virtual void StopSpeaking() OVERRIDE {}
#endif  // defined(OS_MACOSX)
#if defined(TOOLKIT_GTK)
  virtual GdkEventButton* GetLastMouseDown() OVERRIDE { return NULL; }
  virtual gfx::NativeView BuildInputMethodsGtkMenu() OVERRIDE {
    return gfx::NativeView();
  }
#endif  // defined(TOOLKIT_GTK)
#if defined(OS_ANDROID)
  virtual void StartContentIntent(const GURL& content_url) OVERRIDE {}
  virtual void SetCachedBackgroundColor(SkColor color) OVERRIDE {}
#endif
  virtual void SetBackground(const SkBitmap& background) OVERRIDE {}
  virtual const SkBitmap& GetBackground() OVERRIDE {
    return sk_bitmap_;
  }
#if defined(OS_WIN) && !defined(USE_AURA)
  virtual void SetClickthroughRegion(SkRegion* region) {}
#endif
  virtual bool LockMouse() OVERRIDE { return false; }
  virtual void UnlockMouse() OVERRIDE {}
  virtual bool IsMouseLocked() OVERRIDE { return false; }

 private:
  mutable TestRenderWidgetHost host_;

  // For const-reference return value.
  SkBitmap sk_bitmap_;

  DISALLOW_COPY_AND_ASSIGN(TestRenderWidgetHostView);
};

class MockNavigationController : public NavigationController {
 public:
  MockNavigationController() {}
  virtual ~MockNavigationController() {}

  // NavigationController:
  virtual WebContents* GetWebContents() const OVERRIDE { return NULL; }
  virtual BrowserContext* GetBrowserContext() const OVERRIDE { return NULL; }
  virtual void SetBrowserContext(BrowserContext* browser_context) OVERRIDE {}
  virtual void Restore(int selected_navigation,
                       RestoreType type,
                       std::vector<NavigationEntry*>* entries) OVERRIDE {}
  virtual NavigationEntry* GetActiveEntry() const OVERRIDE { return NULL; }
  virtual NavigationEntry* GetVisibleEntry() const OVERRIDE { return NULL; }
  virtual int GetCurrentEntryIndex() const OVERRIDE { return 0; }
  virtual NavigationEntry* GetLastCommittedEntry() const OVERRIDE {
    return NULL;
  }
  virtual int GetLastCommittedEntryIndex() const OVERRIDE { return 0; }
  virtual bool CanViewSource() const OVERRIDE { return false; }
  virtual int GetEntryCount() const OVERRIDE { return 0; }
  virtual NavigationEntry* GetEntryAtIndex(int index) const OVERRIDE {
    return NULL;
  }
  virtual NavigationEntry* GetEntryAtOffset(int offset) const OVERRIDE {
    return NULL;
  }
  virtual void DiscardNonCommittedEntries() OVERRIDE {}
  virtual NavigationEntry* GetPendingEntry() const OVERRIDE { return NULL; }
  virtual int GetPendingEntryIndex() const OVERRIDE { return 0; }
  virtual NavigationEntry* GetTransientEntry() const OVERRIDE { return NULL; }
  virtual void SetTransientEntry(NavigationEntry* entry) OVERRIDE {}
  virtual void LoadURL(const GURL& url,
                       const Referrer& referrer,
                       PageTransition type,
                       const std::string& extra_headers) OVERRIDE {}
  virtual void LoadURLWithParams(const LoadURLParams& params) OVERRIDE {}
  virtual void LoadIfNecessary() OVERRIDE {}
  virtual bool CanGoBack() const OVERRIDE { return false; }
  virtual bool CanGoForward() const OVERRIDE { return false; }
  virtual bool CanGoToOffset(int offset) const OVERRIDE { return false; }
  virtual void GoBack() OVERRIDE {}
  virtual void GoForward() OVERRIDE {}
  virtual void GoToIndex(int index) OVERRIDE {}
  virtual void GoToOffset(int offset) OVERRIDE {}
  virtual void Reload(bool check_for_repost) OVERRIDE {}
  virtual void ReloadIgnoringCache(bool check_for_repost) OVERRIDE {}
  virtual void ReloadOriginalRequestURL(bool check_for_repost) OVERRIDE {}
  virtual void RemoveEntryAtIndex(int index) OVERRIDE {}
#if !defined(OS_IOS)
  virtual const SessionStorageNamespaceMap&
      GetSessionStorageNamespaceMap() const OVERRIDE {
    return session_storage_namespace_map_;
  }
  virtual SessionStorageNamespace*
      GetDefaultSessionStorageNamespace() OVERRIDE { return NULL; }
#endif
  virtual void SetMaxRestoredPageID(int32 max_id) OVERRIDE {}
  virtual int32 GetMaxRestoredPageID() const OVERRIDE { return 0; }
  virtual bool NeedsReload() const OVERRIDE { return false; }
  virtual void CancelPendingReload() OVERRIDE {}
  virtual void ContinuePendingReload() OVERRIDE {}
  virtual bool IsInitialNavigation() OVERRIDE { return false; }
  virtual void NotifyEntryChanged(const NavigationEntry* entry,
                                  int index) OVERRIDE {}
  virtual void CopyStateFrom(const NavigationController& source) OVERRIDE {}
  virtual void CopyStateFromAndPrune(NavigationController* source) OVERRIDE {}
  virtual void PruneAllButActive() OVERRIDE {}
  virtual void ClearAllScreenshots() OVERRIDE {}

 private:
  // For const-reference return value.
  SessionStorageNamespaceMap session_storage_namespace_map_;

  DISALLOW_COPY_AND_ASSIGN(MockNavigationController);
};

class TestWebContents : public WebContents {
 public:
  TestWebContents(const gfx::Rect& content_bounds,
                  const gfx::Rect& screen_bounds,
                  const gfx::Rect& available_screen_bounds)
      : view_(content_bounds),
        host_view_(screen_bounds, available_screen_bounds) {}
  virtual ~TestWebContents() {}

  virtual WebContentsView* GetView() const OVERRIDE {
    return &view_;
  }

  virtual RenderWidgetHostView* GetRenderWidgetHostView() const OVERRIDE {
    return &host_view_;
  }

  // The rest of WebContents:
  virtual WebContentsDelegate* GetDelegate() OVERRIDE { return NULL; }
  virtual void SetDelegate(WebContentsDelegate* delegate) OVERRIDE {}
  virtual NavigationController& GetController() OVERRIDE {
    return navigation_controller_;
  }
  virtual const NavigationController& GetController() const OVERRIDE {
    return navigation_controller_;
  }
  virtual BrowserContext* GetBrowserContext() const OVERRIDE { return NULL; }
  virtual const GURL& GetURL() const OVERRIDE { return gurl_; }
  virtual RenderProcessHost* GetRenderProcessHost() const OVERRIDE {
    return NULL;
  }
  virtual RenderViewHost* GetRenderViewHost() const OVERRIDE {
    return NULL;
  }
  virtual void GetRenderViewHostAtPosition(
      int x,
      int y,
      const GetRenderViewHostCallback& callback) OVERRIDE {}
  virtual WebContents* GetEmbedderWebContents() const OVERRIDE {
    return NULL;
  }
  virtual int GetEmbeddedInstanceID() const OVERRIDE { return 0; }
  virtual int GetRoutingID() const OVERRIDE { return 0; }
  virtual WebUI* CreateWebUI(const GURL& url) OVERRIDE { return NULL; }
  virtual WebUI* GetWebUI() const OVERRIDE { return NULL; }
  virtual WebUI* GetCommittedWebUI() const OVERRIDE { return NULL; }
  virtual void SetUserAgentOverride(const std::string& override) OVERRIDE {}
  virtual const std::string& GetUserAgentOverride() const OVERRIDE {
    return string_;
  }
  virtual const string16& GetTitle() const OVERRIDE { return string16_; }
  virtual int32 GetMaxPageID() OVERRIDE { return 0; }
  virtual int32 GetMaxPageIDForSiteInstance(
      SiteInstance* site_instance) OVERRIDE { return 0; }
  virtual SiteInstance* GetSiteInstance() const OVERRIDE { return NULL; }
  virtual SiteInstance* GetPendingSiteInstance() const OVERRIDE { return NULL; }
  virtual bool IsLoading() const OVERRIDE { return false; }
  virtual bool IsWaitingForResponse() const OVERRIDE { return false; }
  virtual const net::LoadStateWithParam& GetLoadState() const OVERRIDE {
    return load_state_with_param_;
  }
  virtual const string16& GetLoadStateHost() const OVERRIDE {
    return string16_;
  }
  virtual uint64 GetUploadSize() const OVERRIDE { return 0; }
  virtual uint64 GetUploadPosition() const OVERRIDE { return 0; }
  virtual const std::string& GetEncoding() const OVERRIDE { return string_; }
  virtual bool DisplayedInsecureContent() const OVERRIDE { return false; }
  virtual void IncrementCapturerCount() OVERRIDE {}
  virtual void DecrementCapturerCount() OVERRIDE {}
  virtual bool IsCrashed() const OVERRIDE { return false; }
  virtual void SetIsCrashed(base::TerminationStatus status,
                            int error_code) OVERRIDE {}
  virtual base::TerminationStatus GetCrashedStatus() const OVERRIDE {
    return base::TerminationStatus();
  }
  virtual bool IsBeingDestroyed() const OVERRIDE { return false; }
  virtual void NotifyNavigationStateChanged(unsigned changed_flags) OVERRIDE {}
  virtual base::TimeTicks GetLastSelectedTime() const OVERRIDE {
    return base::TimeTicks();
  }
  virtual void WasShown() OVERRIDE {}
  virtual void WasHidden() OVERRIDE {}
  virtual bool NeedToFireBeforeUnload() OVERRIDE { return false; }
  virtual void Stop() OVERRIDE {}
  virtual WebContents* Clone() OVERRIDE { return NULL; }
  virtual gfx::NativeView GetContentNativeView() const OVERRIDE {
    return gfx::NativeView();
  }
  virtual gfx::NativeView GetNativeView() const OVERRIDE {
    return gfx::NativeView();
  }
  virtual void GetContainerBounds(gfx::Rect* out) const OVERRIDE {}
  virtual void Focus() OVERRIDE {}
  virtual void FocusThroughTabTraversal(bool reverse) OVERRIDE {}
  virtual bool ShowingInterstitialPage() const OVERRIDE { return false; }
  virtual InterstitialPage* GetInterstitialPage() const OVERRIDE {
    return NULL;
  }
  virtual bool IsSavable() OVERRIDE { return false; }
  virtual void OnSavePage() OVERRIDE {}
  virtual bool SavePage(const base::FilePath& main_file,
                        const base::FilePath& dir_path,
                        SavePageType save_type) OVERRIDE { return false; }
  virtual void GenerateMHTML(
      const base::FilePath& file,
      const base::Callback<void(const base::FilePath&, int64)>& callback)
          OVERRIDE {}
  virtual bool IsActiveEntry(int32 page_id) OVERRIDE { return false; }

  virtual const std::string& GetContentsMimeType() const OVERRIDE {
    return string_;
  }
  virtual bool WillNotifyDisconnection() const OVERRIDE { return false; }
  virtual void SetOverrideEncoding(const std::string& encoding) OVERRIDE {}
  virtual void ResetOverrideEncoding() OVERRIDE {}
  virtual RendererPreferences* GetMutableRendererPrefs() OVERRIDE {
    return NULL;
  }
  virtual void SetNewTabStartTime(const base::TimeTicks& time) OVERRIDE {}
  virtual base::TimeTicks GetNewTabStartTime() const OVERRIDE {
    return base::TimeTicks();
  }
  virtual void Close() OVERRIDE {}
  virtual void OnCloseStarted() OVERRIDE {}
  virtual bool ShouldAcceptDragAndDrop() const OVERRIDE { return false; }
  virtual void SystemDragEnded() OVERRIDE {}
  virtual void UserGestureDone() OVERRIDE {}
  virtual void SetClosedByUserGesture(bool value) OVERRIDE {}
  virtual bool GetClosedByUserGesture() const OVERRIDE { return false; }
  virtual double GetZoomLevel() const OVERRIDE { return 0.0; }
  virtual int GetZoomPercent(bool* enable_increment,
                             bool* enable_decrement) const OVERRIDE {
    return 0;
  }
  virtual void ViewSource() OVERRIDE {}
  virtual void ViewFrameSource(const GURL& url,
                               const std::string& content_state) OVERRIDE {}
  virtual int GetMinimumZoomPercent() const OVERRIDE { return 0; }
  virtual int GetMaximumZoomPercent() const OVERRIDE { return 0; }
  virtual gfx::Size GetPreferredSize() const OVERRIDE { return gfx::Size(); }
  virtual int GetContentRestrictions() const OVERRIDE { return 0; }
  virtual WebUI::TypeID GetWebUITypeForCurrentState() OVERRIDE {
    return WebUI::TypeID();
  }
  virtual WebUI* GetWebUIForCurrentState() OVERRIDE { return NULL; }
  virtual bool GotResponseToLockMouseRequest(bool allowed) OVERRIDE {
    return false;
  }
  virtual bool HasOpener() const OVERRIDE { return false; }
  virtual void DidChooseColorInColorChooser(int color_chooser_id,
                                            SkColor color) OVERRIDE {}
  virtual void DidEndColorChooser(int color_chooser_id) OVERRIDE {}
  virtual bool FocusLocationBarByDefault() OVERRIDE { return false; }
  virtual void SetFocusToLocationBar(bool select_all) OVERRIDE {}
  virtual int DownloadFavicon(
      const GURL& url, int image_size,
      const FaviconDownloadCallback& callback) OVERRIDE { return 0; }
  virtual WebContents* OpenURL(const OpenURLParams& params) OVERRIDE {
    return NULL;
  }
  virtual bool Send(IPC::Message* msg) OVERRIDE { return false; }

 private:
  mutable TestWebContentsView view_;
  mutable TestRenderWidgetHostView host_view_;

  // For const-reference return values.
  std::string string_;
  string16 string16_;
  GURL gurl_;
  net::LoadStateWithParam load_state_with_param_;
  MockNavigationController navigation_controller_;

  DISALLOW_COPY_AND_ASSIGN(TestWebContents);
};

}  // namespace

class AutofillRiskFingerprintTest : public InProcessBrowserTest {
 public:
  AutofillRiskFingerprintTest()
      : kWindowBounds(2, 3, 5, 7),
        kContentBounds(11, 13, 17, 37),
        kScreenBounds(0, 0, 101, 71),
        kAvailableScreenBounds(0, 11, 101, 60),
        kUnavailableScreenBounds(0, 0, 101, 11),
        message_loop_(MessageLoop::TYPE_UI) {}

  void GetFingerprintTestCallback(scoped_ptr<Fingerprint> fingerprint) {
    // TODO(isherman): Investigating http://crbug.com/174296
    LOG(WARNING) << "Callback called.";

    // Verify that all fields Chrome can fill have been filled.
    ASSERT_TRUE(fingerprint->has_machine_characteristics());
    const Fingerprint_MachineCharacteristics& machine =
        fingerprint->machine_characteristics();
    ASSERT_TRUE(machine.has_operating_system_build());
    ASSERT_TRUE(machine.has_browser_install_time_hours());
    ASSERT_GT(machine.font_size(), 0);
    ASSERT_GT(machine.plugin_size(), 0);
    ASSERT_TRUE(machine.has_utc_offset_ms());
    ASSERT_TRUE(machine.has_browser_language());
    ASSERT_GT(machine.requested_language_size(), 0);
    ASSERT_TRUE(machine.has_charset());
    ASSERT_TRUE(machine.has_screen_count());
    ASSERT_TRUE(machine.has_screen_size());
    ASSERT_TRUE(machine.screen_size().has_width());
    ASSERT_TRUE(machine.screen_size().has_height());
    ASSERT_TRUE(machine.has_screen_color_depth());
    ASSERT_TRUE(machine.has_unavailable_screen_size());
    ASSERT_TRUE(machine.unavailable_screen_size().has_width());
    ASSERT_TRUE(machine.unavailable_screen_size().has_height());
    ASSERT_TRUE(machine.has_user_agent());
    ASSERT_TRUE(machine.has_cpu());
    ASSERT_TRUE(machine.cpu().has_vendor_name());
    ASSERT_TRUE(machine.cpu().has_brand());
    ASSERT_TRUE(machine.has_ram());
    ASSERT_TRUE(machine.has_graphics_card());
    ASSERT_TRUE(machine.graphics_card().has_vendor_id());
    ASSERT_TRUE(machine.graphics_card().has_device_id());
    ASSERT_TRUE(machine.has_browser_build());

    ASSERT_TRUE(fingerprint->has_transient_state());
    const Fingerprint_TransientState& transient_state =
        fingerprint->transient_state();
    ASSERT_TRUE(transient_state.has_inner_window_size());
    ASSERT_TRUE(transient_state.has_outer_window_size());
    ASSERT_TRUE(transient_state.inner_window_size().has_width());
    ASSERT_TRUE(transient_state.inner_window_size().has_height());
    ASSERT_TRUE(transient_state.outer_window_size().has_width());
    ASSERT_TRUE(transient_state.outer_window_size().has_height());

    ASSERT_TRUE(fingerprint->has_metadata());
    ASSERT_TRUE(fingerprint->metadata().has_timestamp_ms());
    ASSERT_TRUE(fingerprint->metadata().has_gaia_id());
    ASSERT_TRUE(fingerprint->metadata().has_fingerprinter_version());

    // Some values have exact known (mocked out) values:
    ASSERT_EQ(2, machine.requested_language_size());
    EXPECT_EQ("en-US", machine.requested_language(0));
    EXPECT_EQ("en", machine.requested_language(1));
    EXPECT_EQ(kCharset, machine.charset());
    EXPECT_EQ(kScreenColorDepth, machine.screen_color_depth());
    EXPECT_EQ(kUnavailableScreenBounds.width(),
              machine.unavailable_screen_size().width());
    EXPECT_EQ(kUnavailableScreenBounds.height(),
              machine.unavailable_screen_size().height());
    EXPECT_EQ(kContentBounds.width(),
              transient_state.inner_window_size().width());
    EXPECT_EQ(kContentBounds.height(),
              transient_state.inner_window_size().height());
    EXPECT_EQ(kWindowBounds.width(),
              transient_state.outer_window_size().width());
    EXPECT_EQ(kWindowBounds.height(),
              transient_state.outer_window_size().height());
    EXPECT_EQ(kGaiaId, fingerprint->metadata().gaia_id());

    // TODO(isherman): Investigating http://crbug.com/174296
    LOG(WARNING) << "Stopping the message loop.";
    message_loop_.Quit();
  }

 protected:
  const gfx::Rect kWindowBounds;
  const gfx::Rect kContentBounds;
  const gfx::Rect kScreenBounds;
  const gfx::Rect kAvailableScreenBounds;
  const gfx::Rect kUnavailableScreenBounds;
  MessageLoop message_loop_;
};

// Test that getting a fingerprint works on some basic level.
IN_PROC_BROWSER_TEST_F(AutofillRiskFingerprintTest, GetFingerprint) {
  TestWebContents web_contents(
      kContentBounds, kScreenBounds, kAvailableScreenBounds);

  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterStringPref(prefs::kDefaultCharset, kCharset);
  prefs.registry()->RegisterStringPref(prefs::kAcceptLanguages,
                                       kAcceptLanguages);

  // TODO(isherman): Investigating http://crbug.com/174296
  LOG(WARNING) << "Loading fingerprint.";
  GetFingerprint(
      kGaiaId, kWindowBounds, web_contents, prefs,
      base::Bind(&AutofillRiskFingerprintTest::GetFingerprintTestCallback,
                 base::Unretained(this)));

  // Wait for the callback to be called.
  // TODO(isherman): Investigating http://crbug.com/174296
  LOG(WARNING) << "Waiting for the callback to be called.";
  message_loop_.Run();
}

}  // namespace risk
}  // namespace autofill
