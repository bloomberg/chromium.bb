// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/aeropeek_manager.h"

#include <dwmapi.h>
#include <shobjidl.h>

#include "app/win/shell.h"
#include "base/command_line.h"
#include "base/scoped_comptr_win.h"
#include "base/scoped_native_library.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "base/win/windows_version.h"
#include "chrome/browser/app_icon_win.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/renderer_host/backing_store.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/tab_contents/thumbnail_generator.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/browser_distribution.h"
#include "gfx/gdi_util.h"
#include "gfx/icon_util.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/win/window_impl.h"
#include "views/widget/widget_win.h"

namespace {

// Macros and COM interfaces used in this file.
// These interface declarations are copied from Windows SDK 7.
// TODO(hbono): Bug 16903: to be deleted when we use Windows SDK 7.

// Windows SDK 7 defines these macros only when _WIN32_WINNT >= 0x0601.
// Since Chrome currently sets _WIN32_WINNT to 0x0600, copy these defines here
// so we can use them.
#ifndef WM_DWMSENDICONICTHUMBNAIL
#define WM_DWMSENDICONICTHUMBNAIL           0x0323
#endif
#ifndef WM_DWMSENDICONICLIVEPREVIEWBITMAP
#define WM_DWMSENDICONICLIVEPREVIEWBITMAP   0x0326
#endif

// COM interfaces defined only in Windows SDK 7.
#ifndef __ITaskbarList2_INTERFACE_DEFINED__
#define __ITaskbarList2_INTERFACE_DEFINED__

// EXTERN_C const IID IID_ITaskbarList2;
MIDL_INTERFACE("602D4995-B13A-429b-A66E-1935E44F4317")
ITaskbarList2 : public ITaskbarList {
 public:
  virtual HRESULT STDMETHODCALLTYPE MarkFullscreenWindow(
      /* [in] */ __RPC__in HWND hwnd,
      /* [in] */ BOOL fFullscreen) = 0;
};

#endif  /* __ITaskbarList2_INTERFACE_DEFINED__ */

#ifndef __ITaskbarList3_INTERFACE_DEFINED__
#define __ITaskbarList3_INTERFACE_DEFINED__

typedef struct tagTHUMBBUTTON {
  DWORD dwMask;
  UINT iId;
  UINT iBitmap;
  HICON hIcon;
  WCHAR szTip[ 260 ];
  DWORD dwFlags;
} THUMBBUTTON;

typedef struct tagTHUMBBUTTON *LPTHUMBBUTTON;

// THUMBBUTTON flags
#define THBF_ENABLED             0x0000
#define THBF_DISABLED            0x0001
#define THBF_DISMISSONCLICK      0x0002
#define THBF_NOBACKGROUND        0x0004
#define THBF_HIDDEN              0x0008
// THUMBBUTTON mask
#define THB_BITMAP          0x0001
#define THB_ICON            0x0002
#define THB_TOOLTIP         0x0004
#define THB_FLAGS           0x0008
#define THBN_CLICKED        0x1800

typedef /* [v1_enum] */ enum TBPFLAG {
  TBPF_NOPROGRESS = 0,
  TBPF_INDETERMINATE = 0x1,
  TBPF_NORMAL = 0x2,
  TBPF_ERROR = 0x4,
  TBPF_PAUSED = 0x8
} TBPFLAG;

// EXTERN_C const IID IID_ITaskbarList3;

MIDL_INTERFACE("ea1afb91-9e28-4b86-90e9-9e9f8a5eefaf")
ITaskbarList3 : public ITaskbarList2 {
 public:
  virtual HRESULT STDMETHODCALLTYPE SetProgressValue(
      /* [in] */ __RPC__in HWND hwnd,
      /* [in] */ ULONGLONG ullCompleted,
      /* [in] */ ULONGLONG ullTotal) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetProgressState(
      /* [in] */ __RPC__in HWND hwnd,
      /* [in] */ TBPFLAG tbpFlags) = 0;
  virtual HRESULT STDMETHODCALLTYPE RegisterTab(
      /* [in] */ __RPC__in HWND hwndTab,
      /* [in] */ __RPC__in HWND hwndMDI) = 0;
  virtual HRESULT STDMETHODCALLTYPE UnregisterTab(
      /* [in] */ __RPC__in HWND hwndTab) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetTabOrder(
      /* [in] */ __RPC__in HWND hwndTab,
      /* [in] */ __RPC__in HWND hwndInsertBefore) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetTabActive(
      /* [in] */ __RPC__in HWND hwndTab,
      /* [in] */ __RPC__in HWND hwndMDI,
      /* [in] */ DWORD dwReserved) = 0;
  virtual HRESULT STDMETHODCALLTYPE ThumbBarAddButtons(
      /* [in] */ __RPC__in HWND hwnd,
      /* [in] */ UINT cButtons,
      /* [size_is][in] */ __RPC__in_ecount_full(cButtons)
      LPTHUMBBUTTON pButton) = 0;
  virtual HRESULT STDMETHODCALLTYPE ThumbBarUpdateButtons(
      /* [in] */ __RPC__in HWND hwnd,
      /* [in] */ UINT cButtons,
      /* [size_is][in] */ __RPC__in_ecount_full(cButtons)
      LPTHUMBBUTTON pButton) = 0;
  virtual HRESULT STDMETHODCALLTYPE ThumbBarSetImageList(
      /* [in] */ __RPC__in HWND hwnd,
      /* [in] */ __RPC__in_opt HIMAGELIST himl) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetOverlayIcon(
      /* [in] */ __RPC__in HWND hwnd,
      /* [in] */ __RPC__in HICON hIcon,
      /* [string][in] */ __RPC__in_string LPCWSTR pszDescription) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetThumbnailTooltip(
      /* [in] */ __RPC__in HWND hwnd,
      /* [string][in] */ __RPC__in_string LPCWSTR pszTip) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetThumbnailClip(
      /* [in] */ __RPC__in HWND hwnd,
      /* [in] */ __RPC__in RECT *prcClip) = 0;
};
#endif  // __ITaskbarList3_INTERFACE_DEFINED__

// END OF WINDOWS SDK 7.0

}  // namespace

namespace {

// Sends a thumbnail bitmap to Windows. Windows assumes this function is called
// when a WM_DWMSENDICONICTHUMBNAIL message sent to a place-holder window. We
// can use DwmInvalidateIconicBitmap() to force Windows to send the message.
HRESULT CallDwmSetIconicThumbnail(HWND window, HBITMAP bitmap, DWORD flags) {
  FilePath dwmapi_path(base::GetNativeLibraryName(L"dwmapi"));
  base::ScopedNativeLibrary dwmapi(dwmapi_path);

  typedef HRESULT (STDAPICALLTYPE *DwmSetIconicThumbnailProc)(
      HWND, HBITMAP, DWORD);
  DwmSetIconicThumbnailProc dwm_set_iconic_thumbnail =
      static_cast<DwmSetIconicThumbnailProc>(
      dwmapi.GetFunctionPointer("DwmSetIconicThumbnail"));

  if (!dwm_set_iconic_thumbnail)
    return E_FAIL;

  return dwm_set_iconic_thumbnail(window, bitmap, flags);
}

// Sends a preview bitmap to Windows. Windows assumes this function is called
// when a WM_DWMSENDICONICLIVEPREVIEWBITMAP message sent to a place-holder
// window.
HRESULT CallDwmSetIconicLivePreviewBitmap(HWND window,
                                          HBITMAP bitmap,
                                          POINT* client,
                                          DWORD flags) {
  FilePath dwmapi_path(base::GetNativeLibraryName(L"dwmapi"));
  base::ScopedNativeLibrary dwmapi(dwmapi_path);

  typedef HRESULT (STDAPICALLTYPE *DwmSetIconicLivePreviewBitmapProc)(
      HWND, HBITMAP, POINT*, DWORD);
  DwmSetIconicLivePreviewBitmapProc dwm_set_live_preview_bitmap =
      static_cast<DwmSetIconicLivePreviewBitmapProc>(
      dwmapi.GetFunctionPointer("DwmSetIconicLivePreviewBitmap"));

  if (!dwm_set_live_preview_bitmap)
    return E_FAIL;

  return dwm_set_live_preview_bitmap(window, bitmap, client, flags);
}

// Invalidates the thumbnail image of the specified place-holder window. (See
// the comments in CallDwmSetIconicThumbnai()).
HRESULT CallDwmInvalidateIconicBitmaps(HWND window) {
  FilePath dwmapi_path(base::GetNativeLibraryName(L"dwmapi"));
  base::ScopedNativeLibrary dwmapi(dwmapi_path);

  typedef HRESULT (STDAPICALLTYPE *DwmInvalidateIconicBitmapsProc)(HWND);
  DwmInvalidateIconicBitmapsProc dwm_invalidate_iconic_bitmaps =
      static_cast<DwmInvalidateIconicBitmapsProc>(
      dwmapi.GetFunctionPointer("DwmInvalidateIconicBitmaps"));

  if (!dwm_invalidate_iconic_bitmaps)
    return E_FAIL;

  return dwm_invalidate_iconic_bitmaps(window);
}

}  // namespace

namespace {

// Tasks used in this file.
// This file uses three I/O tasks to implement AeroPeek:
// * RegisterThumbnailTask
//   Register a tab into the thumbnail list of Windows.
// * SendThumbnailTask
//   Create a thumbnail image and send it to Windows.
// * SendLivePreviewTask
//   Create a preview image and send it to Windows.
// These I/O tasks indirectly access the specified tab through the
// AeroPeekWindowDelegate interface to prevent these tasks from accessing the
// deleted tabs.

// A task that registers a thumbnail window as a child of the specified
// browser application.
class RegisterThumbnailTask : public Task {
 public:
  RegisterThumbnailTask(HWND frame_window, HWND window, bool active)
      : frame_window_(frame_window),
        window_(window),
        active_(active) {
  }

 private:
  void Run() {
    // Set the App ID of the browser for this place-holder window to tell
    // that this window is a child of the browser application, i.e. to tell
    // that this thumbnail window should be displayed when we hover the
    // browser icon in the taskbar.
    // TODO(mattm): This should use ShellIntegration::GetChromiumAppId to work
    // properly with multiple profiles.
    app::win::SetAppIdForWindow(
        BrowserDistribution::GetDistribution()->GetBrowserAppId(), window_);

    // Register this place-holder window to the taskbar as a child of
    // the browser window and add it to the end of its tab list.
    // Correctly, this registration should be called after this browser window
    // receives a registered window message "TaskbarButtonCreated", which
    // means that Windows creates a taskbar button for this window in its
    // taskbar. But it seems to be OK to register it without checking the
    // message.
    // TODO(hbono): we need to check this registered message?
    ScopedComPtr<ITaskbarList3> taskbar;
    if (FAILED(taskbar.CreateInstance(CLSID_TaskbarList, NULL,
                                      CLSCTX_INPROC_SERVER)) ||
        FAILED(taskbar->HrInit()) ||
        FAILED(taskbar->RegisterTab(window_, frame_window_)) ||
        FAILED(taskbar->SetTabOrder(window_, NULL)))
      return;
    if (active_)
      taskbar->SetTabActive(window_, frame_window_, 0);
  }

 private:
  // An application window to which we are going to register a tab window.
  // This "application window" is a browser frame in terms of Chrome.
  HWND frame_window_;

  // A tab window.
  // After we register this window as a child of the above application window,
  // Windows sends AeroPeek events to this window.
  // It seems this window MUST be a tool window.
  HWND window_;

  // Whether or not we need to activate this tab by default.
  bool active_;
};

// A task which creates a thumbnail image used by AeroPeek and sends it to
// Windows.
class SendThumbnailTask : public Task {
 public:
  SendThumbnailTask(HWND aeropeek_window,
                    const gfx::Rect& content_bounds,
                    const gfx::Size& aeropeek_size,
                    const SkBitmap& tab_bitmap,
                    base::WaitableEvent* ready)
      : aeropeek_window_(aeropeek_window),
        content_bounds_(content_bounds),
        aeropeek_size_(aeropeek_size),
        tab_bitmap_(tab_bitmap),
        ready_(ready) {
  }

  ~SendThumbnailTask() {
    if (ready_)
      ready_->Signal();
  }

 private:
  void Run() {
    // Calculate the size of the aeropeek thumbnail and resize the tab bitmap
    // to the size. When the given bitmap is an empty bitmap, we create a dummy
    // bitmap from the content-area rectangle to create a DIB. (We don't need to
    // allocate pixels for this case since we don't use them.)
    gfx::Size thumbnail_size;
    SkBitmap thumbnail_bitmap;

    if (tab_bitmap_.isNull() || tab_bitmap_.empty()) {
      GetThumbnailSize(content_bounds_.width(), content_bounds_.height(),
                       &thumbnail_size);

      thumbnail_bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                                 thumbnail_size.width(),
                                 thumbnail_size.height());
    } else {
      GetThumbnailSize(tab_bitmap_.width(), tab_bitmap_.height(),
                       &thumbnail_size);

      thumbnail_bitmap = skia::ImageOperations::Resize(
          tab_bitmap_,
          skia::ImageOperations::RESIZE_LANCZOS3,
          thumbnail_size.width(),
          thumbnail_size.height());
    }

    // Create a DIB, copy the resized image, and send the DIB to Windows.
    // We can delete this DIB after sending it to Windows since Windows creates
    // a copy of the DIB and use it.
    base::win::ScopedHDC hdc(CreateCompatibleDC(NULL));
    if (!hdc.Get()) {
      LOG(ERROR) << "cannot create a memory DC: " << GetLastError();
      return;
    }

    BITMAPINFOHEADER header;
    gfx::CreateBitmapHeader(thumbnail_size.width(), thumbnail_size.height(),
                            &header);

    void* bitmap_data = NULL;
    base::win::ScopedBitmap bitmap(
        CreateDIBSection(hdc,
                         reinterpret_cast<BITMAPINFO*>(&header),
                         DIB_RGB_COLORS,
                         &bitmap_data,
                         NULL,
                         0));

    if (!bitmap.Get() || !bitmap_data) {
      LOG(ERROR) << "cannot create a bitmap: " << GetLastError();
      return;
    }

    SkAutoLockPixels lock(thumbnail_bitmap);
    int* content_pixels = reinterpret_cast<int*>(bitmap_data);
    for (int y = 0; y < thumbnail_size.height(); ++y) {
      for (int x = 0; x < thumbnail_size.width(); ++x) {
        content_pixels[y * thumbnail_size.width() + x] =
            GetPixel(thumbnail_bitmap, x, y);
      }
    }

    HRESULT result = CallDwmSetIconicThumbnail(aeropeek_window_, bitmap, 0);
    if (FAILED(result))
      LOG(ERROR) << "cannot set a tab thumbnail: " << result;
  }

  // Calculates the thumbnail size sent to Windows so we can preserve the pixel
  // aspect-ratio of the source bitmap. Since Windows returns an error when we
  // send an image bigger than the given size, we decrease either the thumbnail
  // width or the thumbnail height so we can fit the longer edge of the source
  // window.
  void GetThumbnailSize(int width, int height, gfx::Size* output) const {
    float thumbnail_width = static_cast<float>(aeropeek_size_.width());
    float thumbnail_height = static_cast<float>(aeropeek_size_.height());
    float source_width = static_cast<float>(width);
    float source_height = static_cast<float>(height);
    DCHECK(source_width && source_height);

    float ratio_width = thumbnail_width / source_width;
    float ratio_height = thumbnail_height / source_height;
    if (ratio_width > ratio_height) {
      thumbnail_width = source_width * ratio_height;
    } else {
      thumbnail_height = source_height * ratio_width;
    }

    output->set_width(static_cast<int>(thumbnail_width));
    output->set_height(static_cast<int>(thumbnail_height));
  }

  // Returns a pixel of the specified bitmap. If this bitmap is a dummy bitmap,
  // this function returns an opaque white pixel instead.
  int GetPixel(const SkBitmap& bitmap, int x, int y) const {
    const int* tab_pixels = reinterpret_cast<const int*>(bitmap.getPixels());
    if (!tab_pixels)
      return 0xFFFFFFFF;
    return tab_pixels[y * bitmap.width() + x];
  }

 private:
  // A window handle to the place-holder window used by AeroPeek.
  HWND aeropeek_window_;

  // The bounding rectangle of the user-perceived content area.
  // This rectangle is used only for creating a fall-back bitmap.
  gfx::Rect content_bounds_;

  // The size of an output image to be sent to Windows.
  gfx::Size aeropeek_size_;

  // The source bitmap.
  SkBitmap tab_bitmap_;

  // An event to notify when this task finishes.
  base::WaitableEvent* ready_;
};

// A task which creates a preview image used by AeroPeek and sends it to
// Windows.
// This task becomes more complicated than SendThumbnailTask because this task
// calculates the rectangle of the user-perceived content area (infobars +
// content area) so Windows can paste the preview image on it.
// This task is used if an AeroPeek window receives a
// WM_DWMSENDICONICLIVEPREVIEWBITMAP message.
class SendLivePreviewTask : public Task {
 public:
  SendLivePreviewTask(HWND aeropeek_window,
                      const gfx::Rect& content_bounds,
                      const SkBitmap& tab_bitmap)
      : aeropeek_window_(aeropeek_window),
        content_bounds_(content_bounds),
        tab_bitmap_(tab_bitmap) {
  }

  ~SendLivePreviewTask() {
  }

 private:
  void Run() {
    // Create a DIB for the user-perceived content area of the tab, copy the
    // tab image into the DIB, and send it to Windows.
    // We don't need to paste this tab image onto the frame image since Windows
    // automatically pastes it for us.
    base::win::ScopedHDC hdc(CreateCompatibleDC(NULL));
    if (!hdc.Get()) {
      LOG(ERROR) << "cannot create a memory DC: " << GetLastError();
      return;
    }

    BITMAPINFOHEADER header;
    gfx::CreateBitmapHeader(content_bounds_.width(), content_bounds_.height(),
                            &header);

    void* bitmap_data = NULL;
    base::win::ScopedBitmap bitmap(
        CreateDIBSection(hdc.Get(),
                         reinterpret_cast<BITMAPINFO*>(&header),
                         DIB_RGB_COLORS, &bitmap_data,
                         NULL, 0));
    if (!bitmap.Get() || !bitmap_data) {
      LOG(ERROR) << "cannot create a bitmap: " << GetLastError();
      return;
    }

    // Copy the tab image onto the DIB.
    SkAutoLockPixels lock(tab_bitmap_);
    int* content_pixels = reinterpret_cast<int*>(bitmap_data);
    for (int y = 0; y < content_bounds_.height(); ++y) {
      for (int x = 0; x < content_bounds_.width(); ++x)
        content_pixels[y * content_bounds_.width() + x] = GetTabPixel(x, y);
    }

    // Send the preview image to Windows.
    // We can set its offset to the top left corner of the user-perceived
    // content area so Windows can paste this bitmap onto the correct
    // position.
    POINT content_offset = {content_bounds_.x(), content_bounds_.y()};
    HRESULT result = CallDwmSetIconicLivePreviewBitmap(
        aeropeek_window_, bitmap, &content_offset, 0);
    if (FAILED(result))
      LOG(ERROR) << "cannot send a content image: " << result;
  }

  int GetTabPixel(int x, int y) const {
    // Return the opaque while pixel to prevent old foreground tab from being
    // shown when we cannot get the specified pixel.
    const int* tab_pixels = reinterpret_cast<int*>(tab_bitmap_.getPixels());
    if (!tab_pixels || x >= tab_bitmap_.width() || y >= tab_bitmap_.height())
      return 0xFFFFFFFF;

    // DWM uses alpha values to distinguish opaque colors and transparent ones.
    // Set the alpha value of this source pixel to prevent the original window
    // from being shown through.
    return 0xFF000000 | tab_pixels[y * tab_bitmap_.width() + x];
  }

 private:
  // A window handle to the AeroPeek window.
  HWND aeropeek_window_;

  // The bounding rectangle of the user-perceived content area. When a tab
  // hasn't been rendered since a browser window is resized, this size doesn't
  // become the same as the bitmap size as shown below.
  //     +----------------------+
  //     |     frame window     |
  //     | +---------------------+
  //     | |     tab contents    |
  //     | +---------------------+
  //     | | old tab contents | |
  //     | +------------------+ |
  //     +----------------------+
  // This rectangle is used for clipping the width and height of the bitmap and
  // cleaning the old tab contents.
  //     +----------------------+
  //     |     frame window     |
  //     | +------------------+ |
  //     | |   tab contents   | |
  //     | +------------------+ |
  //     | |      blank       | |
  //     | +------------------+ |
  //     +----------------------+
  gfx::Rect content_bounds_;

  // The bitmap of the source tab.
  SkBitmap tab_bitmap_;
};

}  // namespace

// A class which implements a place-holder window used by AeroPeek.
// The major work of this class are:
// * Updating the status of Tab Thumbnails;
// * Receiving messages from Windows, and;
// * Translating received messages for TabStrip.
// This class is used by the AeroPeekManager class, which is a proxy
// between TabStrip and Windows 7.
class AeroPeekWindow : public ui::WindowImpl {
 public:
  AeroPeekWindow(HWND frame_window,
                 AeroPeekWindowDelegate* delegate,
                 int tab_id,
                 bool tab_active,
                 const std::wstring& title,
                 const SkBitmap& favicon_bitmap);
  ~AeroPeekWindow();

  // Activates or deactivates this window.
  // This window uses this information not only for highlighting the selected
  // tab when Windows shows the thumbnail list, but also for preventing us
  // from rendering AeroPeek images for deactivated windows so often.
  void Activate();
  void Deactivate();

  // Updates the image of this window.
  // When the AeroPeekManager class calls this function, this window starts
  // a task which updates its thumbnail image.
  // NOTE: to prevent sending lots of tasks that update the thumbnail images
  // and hurt the system performance, we post a task only when |is_loading| is
  // false for non-active tabs. (On the other hand, we always post an update
  // task for an active tab as IE8 does.)
  void Update(bool is_loading);

  // Destroys this window.
  // This function removes this window from the thumbnail list and deletes
  // all the resources attached to this window, i.e. this object is not valid
  // any longer after calling this function.
  void Destroy();

  // Updates the title of this window.
  // This function just sends a WM_SETTEXT message to update the window title.
  void SetTitle(const std::wstring& title);

  // Updates the icon used for AeroPeek. Unlike SetTitle(), this function just
  // saves a copy of the given bitmap since it takes time to create a Windows
  // icon from this bitmap set it as the window icon. We will create a Windows
  // when Windows sends a WM_GETICON message to retrieve it.
  void SetFavIcon(const SkBitmap& favicon);

  // Returns the tab ID associated with this window.
  int tab_id() { return tab_id_; }

  // Message handlers.
  BEGIN_MSG_MAP_EX(TabbedThumbnailWindow)
    MESSAGE_HANDLER_EX(WM_DWMSENDICONICTHUMBNAIL, OnDwmSendIconicThumbnail)
    MESSAGE_HANDLER_EX(WM_DWMSENDICONICLIVEPREVIEWBITMAP,
                       OnDwmSendIconicLivePreviewBitmap)

    MSG_WM_ACTIVATE(OnActivate)
    MSG_WM_CLOSE(OnClose)
    MSG_WM_CREATE(OnCreate)
    MSG_WM_GETICON(OnGetIcon)
  END_MSG_MAP()

 private:
  // Updates the thumbnail image of this window.
  // This function is a wrapper function of CallDwmInvalidateIconicBitmaps()
  // but it invalidates the thumbnail only when |ready_| is signaled to prevent
  // us from posting two or more tasks.
  void UpdateThumbnail();

  // Returns the user-perceived content area.
  gfx::Rect GetContentBounds() const;

  // Message-handler functions.
  // Called when a window has been created.
  LRESULT OnCreate(LPCREATESTRUCT create_struct);

  // Called when this thumbnail window is activated, i.e. a user clicks this
  // thumbnail window.
  void OnActivate(UINT action, BOOL minimized, HWND window);

  // Called when this thumbnail window is closed, i.e. a user clicks the close
  // button of this thumbnail window.
  void OnClose();

  // Called when Windows needs a thumbnail image for this thumbnail window.
  // Windows can send a WM_DWMSENDICONICTHUMBNAIL message anytime when it
  // needs the thumbnail bitmap for this place-holder window (e.g. when we
  // register this place-holder window to Windows, etc.)
  // When this window receives a WM_DWMSENDICONICTHUMBNAIL message, it HAS TO
  // create a thumbnail bitmap and send it to Windows through a
  // DwmSendIconicThumbnail() call. (Windows shows a "page-loading" animation
  // while it waits for a thumbnail bitmap.)
  LRESULT OnDwmSendIconicThumbnail(UINT message,
                                   WPARAM wparam,
                                   LPARAM lparam);

  // Called when Windows needs a preview image for this thumbnail window.
  // Same as above, Windows can send a WM_DWMSENDICONICLIVEPREVIEWBITMAP
  // message anytime when it needs a preview bitmap and we have to create and
  // send the bitmap when it needs it.
  LRESULT OnDwmSendIconicLivePreviewBitmap(UINT message,
                                           WPARAM wparam,
                                           LPARAM lparam);

  // Called when Windows needs an icon for this thumbnail window.
  // Windows sends a WM_GETICON message with ICON_SMALL when it needs an
  // AeroPeek icon. we handle WM_GETICON messages by ourselves so we can create
  // a custom icon from a favicon only when Windows need it.
  HICON OnGetIcon(UINT index);

 private:
  // An application window which owns this tab.
  // We show this thumbnail image of this window when a user hovers a mouse
  // cursor onto the taskbar icon of this application window.
  HWND frame_window_;

  // An interface which dispatches events received from Window.
  // This window notifies events received from Windows to TabStrip through
  // this interface.
  // We should not directly access TabContents members since Windows may send
  // AeroPeek events to a tab closed by Chrome.
  // To prevent such race condition, we get access to TabContents through
  // AeroPeekManager.
  AeroPeekWindowDelegate* delegate_;

  // A tab ID associated with this window.
  int tab_id_;

  // A flag that represents whether or not this tab is active.
  // This flag is used for preventing us from updating the thumbnail images
  // when this window is not active.
  bool tab_active_;

  // An event that represents whether or not we can post a task which updates
  // the thumbnail image of this window.
  // We post a task only when this event is signaled.
  base::WaitableEvent ready_to_update_thumbnail_;

  // The title of this tab.
  std::wstring title_;

  // The favicon for this tab.
  SkBitmap favicon_bitmap_;
  base::win::ScopedHICON favicon_;

  // The icon used by the frame window.
  // This icon is used when this tab doesn't have a favicon.
  HICON frame_icon_;

  DISALLOW_COPY_AND_ASSIGN(AeroPeekWindow);
};

AeroPeekWindow::AeroPeekWindow(HWND frame_window,
                               AeroPeekWindowDelegate* delegate,
                               int tab_id,
                               bool tab_active,
                               const std::wstring& title,
                               const SkBitmap& favicon_bitmap)
    : frame_window_(frame_window),
      delegate_(delegate),
      tab_id_(tab_id),
      tab_active_(tab_active),
      ready_to_update_thumbnail_(false, true),
      title_(title),
      favicon_bitmap_(favicon_bitmap),
      frame_icon_(NULL) {
  // Set the class styles and window styles for this thumbnail window.
  // An AeroPeek window should be a tool window. (Otherwise,
  // Windows doesn't send WM_DWMSENDICONICTHUMBNAIL messages.)
  set_initial_class_style(0);
  set_window_style(WS_POPUP | WS_BORDER | WS_SYSMENU | WS_CAPTION);
  set_window_ex_style(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE);
}

AeroPeekWindow::~AeroPeekWindow() {
}

void AeroPeekWindow::Activate() {
  tab_active_ = true;

  // Create a place-holder window and add it to the tab list if it has not been
  // created yet. (This case happens when we re-attached a detached window.)
  if (!IsWindow(hwnd())) {
    Update(false);
    return;
  }

  // Notify Windows to set the thumbnail focus to this window.
  ScopedComPtr<ITaskbarList3> taskbar;
  HRESULT result = taskbar.CreateInstance(CLSID_TaskbarList, NULL,
                                          CLSCTX_INPROC_SERVER);
  if (FAILED(result)) {
    LOG(ERROR) << "failed creating an ITaskbarList3 interface.";
    return;
  }

  result = taskbar->HrInit();
  if (FAILED(result)) {
    LOG(ERROR) << "failed initializing an ITaskbarList3 interface.";
    return;
  }

  result = taskbar->ActivateTab(hwnd());
  if (FAILED(result)) {
    LOG(ERROR) << "failed activating a thumbnail window.";
    return;
  }

  // Update the thumbnail image to the up-to-date one.
  UpdateThumbnail();
}

void AeroPeekWindow::Deactivate() {
  tab_active_ = false;
}

void AeroPeekWindow::Update(bool is_loading) {
  // Create a place-holder window used by AeroPeek if it has not been created
  // so Windows can send events used by AeroPeek to this window.
  // Windows automatically sends a WM_DWMSENDICONICTHUMBNAIL message after this
  // window is registered to Windows. So, we don't have to invalidate the
  // thumbnail image of this window now.
  if (!hwnd()) {
    gfx::Rect bounds;
    WindowImpl::Init(frame_window_, bounds);
    return;
  }

  // Invalidate the thumbnail image of this window.
  // When we invalidate the thumbnail image, we HAVE TO handle a succeeding
  // WM_DWMSENDICONICTHUMBNAIL message and update the thumbnail image with a
  // DwmSetIconicThumbnail() call. So, we should not call this function when
  // we don't have enough information to create a thumbnail.
  if (tab_active_ || !is_loading)
    UpdateThumbnail();
}

void AeroPeekWindow::Destroy() {
  if (!IsWindow(hwnd()))
    return;

  // Remove this window from the tab list of Windows.
  ScopedComPtr<ITaskbarList3> taskbar;
  HRESULT result = taskbar.CreateInstance(CLSID_TaskbarList, NULL,
                                          CLSCTX_INPROC_SERVER);
  if (FAILED(result))
    return;

  result = taskbar->HrInit();
  if (FAILED(result))
    return;

  result = taskbar->UnregisterTab(hwnd());

  // Destroy this window.
  DestroyWindow(hwnd());
}

void AeroPeekWindow::SetTitle(const std::wstring& title) {
  title_ = title;
}

void AeroPeekWindow::SetFavIcon(const SkBitmap& favicon) {
  favicon_bitmap_ = favicon;
}

void AeroPeekWindow::UpdateThumbnail() {
  // We post a task to actually create a new thumbnail. So, this function may
  // be called while we are creating a thumbnail. To prevent this window from
  // posting two or more tasks, we don't invalidate the current thumbnail
  // when this event is not signaled.
  if (ready_to_update_thumbnail_.IsSignaled())
    CallDwmInvalidateIconicBitmaps(hwnd());
}

gfx::Rect AeroPeekWindow::GetContentBounds() const {
  RECT content_rect;
  GetClientRect(frame_window_, &content_rect);

  gfx::Insets content_insets;
  delegate_->GetContentInsets(&content_insets);

  gfx::Rect content_bounds(content_rect);
  content_bounds.Inset(content_insets.left(),
                       content_insets.top(),
                       content_insets.right(),
                       content_insets.bottom());
  return content_bounds;
}

// message handlers

void AeroPeekWindow::OnActivate(UINT action,
                                BOOL minimized,
                                HWND window) {
  // Windows sends a WM_ACTIVATE message not only when a user clicks this
  // window (i.e. this window gains the thumbnail focus) but also a user clicks
  // another window (i.e. this window loses the thumbnail focus.)
  // Return when this window loses the thumbnail focus since we don't have to
  // do anything for this case.
  if (action == WA_INACTIVE)
    return;

  // Ask Chrome to activate the tab associated with this thumbnail window.
  // Since TabStripModel calls AeroPeekManager::TabSelectedAt() when it
  // finishes activating the tab. We will move the tab focus of AeroPeek there.
  if (delegate_)
    delegate_->ActivateTab(tab_id_);
}

LRESULT AeroPeekWindow::OnCreate(LPCREATESTRUCT create_struct) {
  // Initialize the window title now since WindowImpl::Init() always calls
  // CreateWindowEx() with its window name NULL.
  if (!title_.empty()) {
    SendMessage(hwnd(), WM_SETTEXT, 0,
                reinterpret_cast<LPARAM>(title_.c_str()));
  }

  // Window attributes for DwmSetWindowAttribute().
  // These enum values are copied from Windows SDK 7 so we can compile this
  // file with or without it.
  // TODO(hbono): Bug 16903: to be deleted when we use Windows SDK 7.
  enum {
    DWMWA_NCRENDERING_ENABLED = 1,
    DWMWA_NCRENDERING_POLICY,
    DWMWA_TRANSITIONS_FORCEDISABLED,
    DWMWA_ALLOW_NCPAINT,
    DWMWA_CAPTION_BUTTON_BOUNDS,
    DWMWA_NONCLIENT_RTL_LAYOUT,
    DWMWA_FORCE_ICONIC_REPRESENTATION,
    DWMWA_FLIP3D_POLICY,
    DWMWA_EXTENDED_FRAME_BOUNDS,
    DWMWA_HAS_ICONIC_BITMAP,
    DWMWA_DISALLOW_PEEK,
    DWMWA_EXCLUDED_FROM_PEEK,
    DWMWA_LAST
  };

  // Set DWM attributes to tell Windows that this window can provide the
  // bitmaps used by AeroPeek.
  BOOL force_iconic_representation = TRUE;
  DwmSetWindowAttribute(hwnd(),
                        DWMWA_FORCE_ICONIC_REPRESENTATION,
                        &force_iconic_representation,
                        sizeof(force_iconic_representation));

  BOOL has_iconic_bitmap = TRUE;
  DwmSetWindowAttribute(hwnd(),
                        DWMWA_HAS_ICONIC_BITMAP,
                        &has_iconic_bitmap,
                        sizeof(has_iconic_bitmap));

  // Post a task that registers this thumbnail window to Windows because it
  // may take some time. (For example, when we create an ITaskbarList3
  // interface for the first time, Windows loads DLLs and we need to wait for
  // some time.)
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      new RegisterThumbnailTask(frame_window_, hwnd(), tab_active_));

  return 0;
}

void AeroPeekWindow::OnClose() {
  // Unregister this window from the tab list of Windows and destroy this
  // window.
  // The resources attached to this object will be deleted when TabStrip calls
  // AeroPeekManager::TabClosingAt(). (Please read the comment in TabClosingAt()
  // for its details.)
  Destroy();

  // Ask AeroPeekManager to close the tab associated with this thumbnail
  // window.
  if (delegate_)
    delegate_->CloseTab(tab_id_);
}

LRESULT AeroPeekWindow::OnDwmSendIconicThumbnail(UINT message,
                                                 WPARAM wparam,
                                                 LPARAM lparam) {
  // Update the window title to synchronize the title.
  SendMessage(hwnd(), WM_SETTEXT, 0, reinterpret_cast<LPARAM>(title_.c_str()));

  // Create an I/O task since it takes long time to resize these images and
  // send them to Windows. This task signals |ready_to_update_thumbnail_| in
  // its destructor to notify us when this task has been finished. (We create an
  // I/O task even when the given thumbnail is empty to stop the "loading"
  // animation.)
  DCHECK(delegate_);

  SkBitmap thumbnail;
  delegate_->GetTabThumbnail(tab_id_, &thumbnail);

  gfx::Size aeropeek_size(HIWORD(lparam), LOWORD(lparam));
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          new SendThumbnailTask(hwnd(),
                                                GetContentBounds(),
                                                aeropeek_size,
                                                thumbnail,
                                                &ready_to_update_thumbnail_));
  return 0;
}

LRESULT AeroPeekWindow::OnDwmSendIconicLivePreviewBitmap(UINT message,
                                                         WPARAM wparam,
                                                         LPARAM lparam) {
  // Same as OnDwmSendIconicThumbnail(), we create an I/O task which creates
  // a preview image used by AeroPeek and send it to Windows. Unlike
  // OnDwmSendIconicThumbnail(), we don't have to use events for preventing this
  // window from sending two or more tasks because Windows doesn't send
  // WM_DWMSENDICONICLIVEPREVIEWBITMAP messages before we send the preview image
  // to Windows.
  DCHECK(delegate_);

  SkBitmap preview;
  delegate_->GetTabPreview(tab_id_, &preview);

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      new SendLivePreviewTask(hwnd(), GetContentBounds(), preview));

  return 0;
}

HICON AeroPeekWindow::OnGetIcon(UINT index) {
  // Return the application icon if this window doesn't have favicons.
  // We save this application icon to avoid calling LoadIcon() twice or more.
  if (favicon_bitmap_.isNull()) {
    if (!frame_icon_) {
      frame_icon_ = GetAppIcon();
    }
    return frame_icon_;
  }

  // Create a Windows icon from SkBitmap and send it to Windows. We set this
  // icon to the ScopedIcon object to delete it in the destructor.
  favicon_.Set(IconUtil::CreateHICONFromSkBitmap(favicon_bitmap_));
  return favicon_.Get();
}

AeroPeekManager::AeroPeekManager(HWND application_window)
    : application_window_(application_window),
      border_left_(0),
      border_top_(0),
      toolbar_top_(0) {
}

AeroPeekManager::~AeroPeekManager() {
  // Delete all AeroPeekWindow objects.
  for (std::list<AeroPeekWindow*>::iterator i = tab_list_.begin();
       i != tab_list_.end(); ++i) {
    AeroPeekWindow* window = *i;
    delete window;
  }
}

void AeroPeekManager::SetContentInsets(const gfx::Insets& insets) {
  content_insets_ = insets;
}

// static
bool AeroPeekManager::Enabled() {
  // We enable our custom AeroPeek only when:
  // * Chrome is running on Windows 7 and Aero is enabled,
  // * Chrome is not launched in application mode, and
  // * Chrome is launched with the "--enable-aero-peek-tabs" option.
  // TODO(hbono): Bug 37957 <http://crbug.com/37957>: find solutions that avoid
  // flooding users with tab thumbnails.
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  return base::win::GetVersion() >= base::win::VERSION_WIN7 &&
      views::WidgetWin::IsAeroGlassEnabled() &&
      !command_line->HasSwitch(switches::kApp) &&
      command_line->HasSwitch(switches::kEnableAeroPeekTabs);
}

void AeroPeekManager::DeleteAeroPeekWindow(int tab_id) {
  // This function does NOT call AeroPeekWindow::Destroy() before deleting
  // the AeroPeekWindow instance.
  for (std::list<AeroPeekWindow*>::iterator i = tab_list_.begin();
       i != tab_list_.end(); ++i) {
    AeroPeekWindow* window = *i;
    if (window->tab_id() == tab_id) {
      tab_list_.erase(i);
      delete window;
      return;
    }
  }
}
AeroPeekWindow* AeroPeekManager::GetAeroPeekWindow(int tab_id) const {
  size_t size = tab_list_.size();
  for (std::list<AeroPeekWindow*>::const_iterator i = tab_list_.begin();
       i != tab_list_.end(); ++i) {
    AeroPeekWindow* window = *i;
    if (window->tab_id() == tab_id)
      return window;
  }
  return NULL;
}

TabContents* AeroPeekManager::GetTabContents(int tab_id) const {
  for (TabContentsIterator iterator; !iterator.done(); ++iterator) {
    TabContents* target_contents = *iterator;
    if (target_contents->controller().session_id().id() == tab_id)
      return target_contents;
  }
  return NULL;
}

int AeroPeekManager::GetTabID(TabContents* contents) const {
  if (!contents)
    return -1;
  return contents->controller().session_id().id();
}

///////////////////////////////////////////////////////////////////////////////
// AeroPeekManager, TabStripModelObserver implementation:

void AeroPeekManager::TabInsertedAt(TabContentsWrapper* contents,
                                    int index,
                                    bool foreground) {
  if (!contents)
    return;

  // If there are not any AeroPeekWindow objects associated with the given
  // tab, Create a new AeroPeekWindow object and add it to the list.
  if (GetAeroPeekWindow(GetTabID(contents->tab_contents())))
    return;

  AeroPeekWindow* window =
      new AeroPeekWindow(application_window_,
                         this,
                         GetTabID(contents->tab_contents()),
                         foreground,
                         contents->tab_contents()->GetTitle(),
                         contents->tab_contents()->GetFavIcon());
  if (!window)
    return;

  tab_list_.push_back(window);
}

void AeroPeekManager::TabClosingAt(TabStripModel* tab_strip_model,
                                   TabContentsWrapper* contents,
                                   int index) {
  if (!contents)
    return;

  // |tab_strip_model| is NULL when this is being called from TabDetachedAt
  // below.
  // Delete the AeroPeekWindow object associated with this tab and all its
  // resources. (AeroPeekWindow::Destory() also removes this tab from the tab
  // list of Windows.)
  AeroPeekWindow* window =
      GetAeroPeekWindow(GetTabID(contents->tab_contents()));
  if (!window)
    return;

  window->Destroy();
  DeleteAeroPeekWindow(GetTabID(contents->tab_contents()));
}

void AeroPeekManager::TabDetachedAt(TabContentsWrapper* contents, int index) {
  if (!contents)
    return;

  // Same as TabClosingAt(), we remove this tab from the tab list and delete
  // its AeroPeekWindow.
  // Chrome will call TabInsertedAt() when this tab is inserted to another
  // TabStrip. We will re-create an AeroPeekWindow object for this tab and
  // re-add it to the tab list there.
  TabClosingAt(NULL, contents, index);
}

void AeroPeekManager::TabSelectedAt(TabContentsWrapper* old_contents,
                                    TabContentsWrapper* new_contents,
                                    int index,
                                    bool user_gesture) {
  // Deactivate the old window in the thumbnail list and activate the new one
  // to synchronize the thumbnail list with TabStrip.
  if (old_contents) {
    AeroPeekWindow* old_window =
        GetAeroPeekWindow(GetTabID(old_contents->tab_contents()));
    if (old_window)
      old_window->Deactivate();
  }

  if (new_contents) {
    AeroPeekWindow* new_window =
        GetAeroPeekWindow(GetTabID(new_contents->tab_contents()));
    if (new_window)
      new_window->Activate();
  }
}

void AeroPeekManager::TabMoved(TabContentsWrapper* contents,
                               int from_index,
                               int to_index,
                               bool pinned_state_changed) {
  // TODO(hbono): we need to reorder the thumbnail list of Windows here?
  // (Unfortunately, it is not so trivial to reorder the thumbnail list when
  // we detach/attach tabs.)
}

void AeroPeekManager::TabChangedAt(TabContentsWrapper* contents,
                                   int index,
                                   TabChangeType change_type) {
  if (!contents)
    return;

  // Retrieve the AeroPeekWindow object associated with this tab, update its
  // title, and post a task that update its thumbnail image if necessary.
  AeroPeekWindow* window =
      GetAeroPeekWindow(GetTabID(contents->tab_contents()));
  if (!window)
    return;

  // Update the title, the favicon, and the thumbnail used for AeroPeek.
  // These function don't actually update the icon and the thumbnail until
  // Windows needs them (e.g. when a user hovers a taskbar icon) to avoid
  // hurting the rendering performance. (These functions just save the
  // information needed for handling update requests from Windows.)
  window->SetTitle(contents->tab_contents()->GetTitle());
  window->SetFavIcon(contents->tab_contents()->GetFavIcon());
  window->Update(contents->tab_contents()->is_loading());
}

///////////////////////////////////////////////////////////////////////////////
// AeroPeekManager, AeroPeekWindowDelegate implementation:

void AeroPeekManager::ActivateTab(int tab_id) {
  // Ask TabStrip to activate this tab.
  // We don't have to update thumbnails now since TabStrip will call
  // TabSelectedAt() when it actually activates this tab.
  TabContents* contents = GetTabContents(tab_id);
  if (contents && contents->delegate())
    contents->delegate()->ActivateContents(contents);
}

void AeroPeekManager::CloseTab(int tab_id) {
  // Ask TabStrip to close this tab.
  // TabStrip will call TabClosingAt() when it actually closes this tab. We
  // will delete the AeroPeekWindow object attached to this tab there.
  TabContents* contents = GetTabContents(tab_id);
  if (contents && contents->delegate())
    contents->delegate()->CloseContents(contents);
}

void AeroPeekManager::GetContentInsets(gfx::Insets* insets) {
  *insets = content_insets_;
}

bool AeroPeekManager::GetTabThumbnail(int tab_id, SkBitmap* thumbnail) {
  DCHECK(thumbnail);

  // Copy the thumbnail image and the favicon of this tab. We will resize the
  // images and send them to Windows.
  TabContents* contents = GetTabContents(tab_id);
  if (!contents)
    return false;

  ThumbnailGenerator* generator = g_browser_process->GetThumbnailGenerator();
  DCHECK(generator);
  *thumbnail = generator->GetThumbnailForRenderer(contents->render_view_host());

  return true;
}

bool AeroPeekManager::GetTabPreview(int tab_id, SkBitmap* preview) {
  DCHECK(preview);

  // Retrieve the BackingStore associated with the given tab and return its
  // SkPlatformCanvas.
  TabContents* contents = GetTabContents(tab_id);
  if (!contents)
    return false;

  RenderViewHost* render_view_host = contents->render_view_host();
  if (!render_view_host)
    return false;

  BackingStore* backing_store = render_view_host->GetBackingStore(false);
  if (!backing_store)
    return false;

  // Create a copy of this BackingStore image.
  // This code is just copied from "thumbnail_generator.cc".
  skia::PlatformCanvas canvas;
  if (!backing_store->CopyFromBackingStore(gfx::Rect(backing_store->size()),
                                           &canvas))
    return false;

  const SkBitmap& bitmap = canvas.getTopPlatformDevice().accessBitmap(false);
  bitmap.copyTo(preview, SkBitmap::kARGB_8888_Config);
  return true;
}
