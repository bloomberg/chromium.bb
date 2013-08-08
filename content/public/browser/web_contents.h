// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_H_

#include <set>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/process/kill.h"
#include "base/strings/string16.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/save_page_type.h"
#include "content/public/browser/web_ui.h"
#include "ipc/ipc_sender.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

namespace base {
class TimeTicks;
}

namespace gfx {
class Rect;
class Size;
}

namespace net {
struct LoadStateWithParam;
}

namespace content {

class BrowserContext;
class InterstitialPage;
class PageState;
class RenderProcessHost;
class RenderViewHost;
class RenderWidgetHostView;
class SiteInstance;
class WebContentsDelegate;
class WebContentsView;
struct RendererPreferences;

// WebContents is the core class in content/. A WebContents renders web content
// (usually HTML) in a rectangular area.
//
// Instantiating one is simple:
//   scoped_ptr<content::WebContents> web_contents(
//       content::WebContents::Create(
//           content::WebContents::CreateParams(browser_context)));
//   gfx::NativeView view = web_contents->GetView()->GetNativeView();
//   // |view| is an HWND, NSView*, GtkWidget*, etc.; insert it into the view
//   // hierarchy wherever it needs to go.
//
// That's it; go to your kitchen, grab a scone, and chill. WebContents will do
// all the multi-process stuff behind the scenes. More details are at
// http://www.chromium.org/developers/design-documents/multi-process-architecture .
//
// Each WebContents has exactly one NavigationController; each
// NavigationController belongs to one WebContents. The NavigationController can
// be obtained from GetController(), and is used to load URLs into the
// WebContents, navigate it backwards/forwards, etc. See navigation_controller.h
// for more details.
class WebContents : public PageNavigator,
                    public IPC::Sender,
                    public base::SupportsUserData {
 public:
  struct CONTENT_EXPORT CreateParams {
    explicit CreateParams(BrowserContext* context);
    CreateParams(BrowserContext* context, SiteInstance* site);

    BrowserContext* browser_context;

    // Specifying a SiteInstance here is optional.  It can be set to avoid an
    // extra process swap if the first navigation is expected to require a
    // privileged process.
    SiteInstance* site_instance;

    WebContents* opener;
    int routing_id;
    int main_frame_routing_id;

    // Initial size of the new WebContent's view. Can be (0, 0) if not needed.
    gfx::Size initial_size;

    // Used to specify the location context which display the new view should
    // belong. This can be NULL if not needed.
    gfx::NativeView context;
  };

  // Creates a new WebContents.
  CONTENT_EXPORT static WebContents* Create(const CreateParams& params);

  // Similar to Create() above but should be used when you need to prepopulate
  // the SessionStorageNamespaceMap of the WebContents. This can happen if
  // you duplicate a WebContents, try to reconstitute it from a saved state,
  // or when you create a new WebContents based on another one (eg., when
  // servicing a window.open() call).
  //
  // You do not want to call this. If you think you do, make sure you completely
  // understand when SessionStorageNamespace objects should be cloned, why
  // they should not be shared by multiple WebContents, and what bad things
  // can happen if you share the object.
  CONTENT_EXPORT static WebContents* CreateWithSessionStorage(
      const CreateParams& params,
      const SessionStorageNamespaceMap& session_storage_namespace_map);

  // Adds/removes a callback called on creation of each new WebContents.
  typedef base::Callback<void(WebContents*)> CreatedCallback;
  CONTENT_EXPORT static void AddCreatedCallback(
      const CreatedCallback& callback);
  CONTENT_EXPORT static void RemoveCreatedCallback(
      const CreatedCallback& callback);

  // Returns a WebContents that wraps the RenderViewHost, or NULL if the
  // render view host's delegate isn't a WebContents.
  CONTENT_EXPORT static WebContents* FromRenderViewHost(
      const RenderViewHost* rvh);

  virtual ~WebContents() {}

  // Intrinsic tab state -------------------------------------------------------

  // Gets/Sets the delegate.
  virtual WebContentsDelegate* GetDelegate() = 0;
  virtual void SetDelegate(WebContentsDelegate* delegate) = 0;

  // Gets the controller for this WebContents.
  virtual NavigationController& GetController() = 0;
  virtual const NavigationController& GetController() const = 0;

  // Returns the user browser context associated with this WebContents (via the
  // NavigationController).
  virtual content::BrowserContext* GetBrowserContext() const = 0;

  // Gets the URL that is currently being displayed, if there is one.
  // This method is deprecated. DO NOT USE! Pick either |GetVisibleURL| or
  // |GetLastCommittedURL| as appropriate.
  virtual const GURL& GetURL() const = 0;

  // Gets the URL currently being displayed in the URL bar, if there is one.
  // This URL might be a pending navigation that hasn't committed yet, so it is
  // not guaranteed to match the current page in this WebContents. A typical
  // example of this is interstitials, which show the URL of the new/loading
  // page (active) but the security context is of the old page (last committed).
  virtual const GURL& GetVisibleURL() const = 0;

  // Gets the last committed URL. It represents the current page that is
  // displayed in  this WebContents. It represents the current security
  // context.
  virtual const GURL& GetLastCommittedURL() const = 0;

  // Return the currently active RenderProcessHost and RenderViewHost. Each of
  // these may change over time.
  virtual RenderProcessHost* GetRenderProcessHost() const = 0;

  // Gets the current RenderViewHost for this tab.
  virtual RenderViewHost* GetRenderViewHost() const = 0;

  typedef base::Callback<void(RenderViewHost* /* render_view_host */,
                              int /* x */,
                              int /* y */)> GetRenderViewHostCallback;
  // Gets the RenderViewHost at coordinates (|x|, |y|) for this WebContents via
  // |callback|.
  // This can be different than the current RenderViewHost if there is a
  // BrowserPlugin at the specified position.
  virtual void GetRenderViewHostAtPosition(
      int x,
      int y,
      const GetRenderViewHostCallback& callback) = 0;

  // Returns the WebContents embedding this WebContents, if any.
  // If this is a top-level WebContents then it returns NULL.
  virtual WebContents* GetEmbedderWebContents() const = 0;

  // Gets the instance ID of the current WebContents if it is embedded
  // within a BrowserPlugin. The instance ID of a WebContents uniquely
  // identifies it within its embedder WebContents.
  virtual int GetEmbeddedInstanceID() const = 0;

  // Gets the current RenderViewHost's routing id. Returns
  // MSG_ROUTING_NONE when there is no RenderViewHost.
  virtual int GetRoutingID() const = 0;

  // Returns the currently active RenderWidgetHostView. This may change over
  // time and can be NULL (during setup and teardown).
  virtual content::RenderWidgetHostView* GetRenderWidgetHostView() const = 0;

  // The WebContentsView will never change and is guaranteed non-NULL.
  virtual WebContentsView* GetView() const = 0;

  // Create a WebUI page for the given url. In most cases, this doesn't need to
  // be called by embedders since content will create its own WebUI objects as
  // necessary. However if the embedder wants to create its own WebUI object and
  // keep track of it manually, it can use this.
  virtual WebUI* CreateWebUI(const GURL& url) = 0;

  // Returns the committed WebUI if one exists, otherwise the pending one.
  virtual WebUI* GetWebUI() const = 0;
  virtual WebUI* GetCommittedWebUI() const = 0;

  // Allows overriding the user agent used for NavigationEntries it owns.
  virtual void SetUserAgentOverride(const std::string& override) = 0;
  virtual const std::string& GetUserAgentOverride() const = 0;

#if defined(OS_WIN) && defined(USE_AURA)
  virtual void SetParentNativeViewAccessible(
      gfx::NativeViewAccessible accessible_parent) = 0;
#endif

  // Tab navigation state ------------------------------------------------------

  // Returns the current navigation properties, which if a navigation is
  // pending may be provisional (e.g., the navigation could result in a
  // download, in which case the URL would revert to what it was previously).
  virtual const string16& GetTitle() const = 0;

  // The max page ID for any page that the current SiteInstance has loaded in
  // this WebContents.  Page IDs are specific to a given SiteInstance and
  // WebContents, corresponding to a specific RenderView in the renderer.
  // Page IDs increase with each new page that is loaded by a tab.
  virtual int32 GetMaxPageID() = 0;

  // The max page ID for any page that the given SiteInstance has loaded in
  // this WebContents.
  virtual int32 GetMaxPageIDForSiteInstance(SiteInstance* site_instance) = 0;

  // Returns the SiteInstance associated with the current page.
  virtual SiteInstance* GetSiteInstance() const = 0;

  // Returns the SiteInstance for the pending navigation, if any.  Otherwise
  // returns the current SiteInstance.
  virtual SiteInstance* GetPendingSiteInstance() const = 0;

  // Return whether this WebContents is loading a resource.
  virtual bool IsLoading() const = 0;

  // Returns whether this WebContents is waiting for a first-response for the
  // main resource of the page.
  virtual bool IsWaitingForResponse() const = 0;

  // Return the current load state and the URL associated with it.
  virtual const net::LoadStateWithParam& GetLoadState() const = 0;
  virtual const string16& GetLoadStateHost() const = 0;

  // Return the upload progress.
  virtual uint64 GetUploadSize() const = 0;
  virtual uint64 GetUploadPosition() const = 0;

  // Returns a set of the site URLs currently committed in this tab.
  virtual std::set<GURL> GetSitesInTab() const = 0;

  // Return the character encoding of the page.
  virtual const std::string& GetEncoding() const = 0;

  // True if this is a secure page which displayed insecure content.
  virtual bool DisplayedInsecureContent() const = 0;

  // Internal state ------------------------------------------------------------

  // Indicates whether the WebContents is being captured (e.g., for screenshots
  // or mirroring).  Increment calls must be balanced with an equivalent number
  // of decrement calls.
  virtual void IncrementCapturerCount() = 0;
  virtual void DecrementCapturerCount() = 0;
  virtual int GetCapturerCount() const = 0;

  // Indicates whether this tab should be considered crashed. The setter will
  // also notify the delegate when the flag is changed.
  virtual bool IsCrashed() const  = 0;
  virtual void SetIsCrashed(base::TerminationStatus status, int error_code) = 0;

  virtual base::TerminationStatus GetCrashedStatus() const = 0;

  // Whether the tab is in the process of being destroyed.
  virtual bool IsBeingDestroyed() const = 0;

  // Convenience method for notifying the delegate of a navigation state
  // change. See InvalidateType enum.
  virtual void NotifyNavigationStateChanged(unsigned changed_flags) = 0;

  // Get the last time that the WebContents was made visible with WasShown()
  virtual base::TimeTicks GetLastSelectedTime() const = 0;

  // Invoked when the WebContents becomes shown/hidden.
  virtual void WasShown() = 0;
  virtual void WasHidden() = 0;

  // Returns true if the before unload and unload listeners need to be
  // fired. The value of this changes over time. For example, if true and the
  // before unload listener is executed and allows the user to exit, then this
  // returns false.
  virtual bool NeedToFireBeforeUnload() = 0;

  // Commands ------------------------------------------------------------------

  // Stop any pending navigation.
  virtual void Stop() = 0;

  // Creates a new WebContents with the same state as this one. The returned
  // heap-allocated pointer is owned by the caller.
  virtual WebContents* Clone() = 0;

  // Views and focus -----------------------------------------------------------
  // Focuses the first (last if |reverse| is true) element in the page.
  // Invoked when this tab is getting the focus through tab traversal (|reverse|
  // is true when using Shift-Tab).
  virtual void FocusThroughTabTraversal(bool reverse) = 0;

  // Interstitials -------------------------------------------------------------

  // Various other systems need to know about our interstitials.
  virtual bool ShowingInterstitialPage() const = 0;

  // Returns the currently showing interstitial, NULL if no interstitial is
  // showing.
  virtual InterstitialPage* GetInterstitialPage() const = 0;

  // Misc state & callbacks ----------------------------------------------------

  // Check whether we can do the saving page operation this page given its MIME
  // type.
  virtual bool IsSavable() = 0;

  // Prepare for saving the current web page to disk.
  virtual void OnSavePage() = 0;

  // Save page with the main HTML file path, the directory for saving resources,
  // and the save type: HTML only or complete web page. Returns true if the
  // saving process has been initiated successfully.
  virtual bool SavePage(const base::FilePath& main_file,
                        const base::FilePath& dir_path,
                        SavePageType save_type) = 0;

  // Saves the given frame's URL to the local filesystem..
  virtual void SaveFrame(const GURL& url,
                         const Referrer& referrer) = 0;

  // Generate an MHTML representation of the current page in the given file.
  virtual void GenerateMHTML(
      const base::FilePath& file,
      const base::Callback<void(
          const base::FilePath& /* path to the MHTML file */,
          int64 /* size of the file */)>& callback) = 0;

  // Returns true if the active NavigationEntry's page_id equals page_id.
  virtual bool IsActiveEntry(int32 page_id) = 0;

  // Returns the contents MIME type after a navigation.
  virtual const std::string& GetContentsMimeType() const = 0;

  // Returns true if this WebContents will notify about disconnection.
  virtual bool WillNotifyDisconnection() const = 0;

  // Override the encoding and reload the page by sending down
  // ViewMsg_SetPageEncoding to the renderer. |UpdateEncoding| is kinda
  // the opposite of this, by which 'browser' is notified of
  // the encoding of the current tab from 'renderer' (determined by
  // auto-detect, http header, meta, bom detection, etc).
  virtual void SetOverrideEncoding(const std::string& encoding) = 0;

  // Remove any user-defined override encoding and reload by sending down
  // ViewMsg_ResetPageEncodingToDefault to the renderer.
  virtual void ResetOverrideEncoding() = 0;

  // Returns the settings which get passed to the renderer.
  virtual content::RendererPreferences* GetMutableRendererPrefs() = 0;

  // Tells the tab to close now. The tab will take care not to close until it's
  // out of nested message loops.
  virtual void Close() = 0;

  // A render view-originated drag has ended. Informs the render view host and
  // WebContentsDelegate.
  virtual void SystemDragEnded() = 0;

  // Notification the user has made a gesture while focus was on the
  // page. This is used to avoid uninitiated user downloads (aka carpet
  // bombing), see DownloadRequestLimiter for details.
  virtual void UserGestureDone() = 0;

  // Indicates if this tab was explicitly closed by the user (control-w, close
  // tab menu item...). This is false for actions that indirectly close the tab,
  // such as closing the window.  The setter is maintained by TabStripModel, and
  // the getter only useful from within TAB_CLOSED notification
  virtual void SetClosedByUserGesture(bool value) = 0;
  virtual bool GetClosedByUserGesture() const = 0;

  // Gets the zoom level for this tab.
  virtual double GetZoomLevel() const = 0;

  // Gets the zoom percent for this tab.
  virtual int GetZoomPercent(bool* enable_increment,
                             bool* enable_decrement) const = 0;

  // Opens view-source tab for this contents.
  virtual void ViewSource() = 0;

  virtual void ViewFrameSource(const GURL& url,
                               const PageState& page_state)= 0;

  // Gets the minimum/maximum zoom percent.
  virtual int GetMinimumZoomPercent() const = 0;
  virtual int GetMaximumZoomPercent() const = 0;

  // Gets the preferred size of the contents.
  virtual gfx::Size GetPreferredSize() const = 0;

  // Called when the reponse to a pending mouse lock request has arrived.
  // Returns true if |allowed| is true and the mouse has been successfully
  // locked.
  virtual bool GotResponseToLockMouseRequest(bool allowed) = 0;

  // Called when the user has selected a color in the color chooser.
  virtual void DidChooseColorInColorChooser(SkColor color) = 0;

  // Called when the color chooser has ended.
  virtual void DidEndColorChooser() = 0;

  // Returns true if the location bar should be focused by default rather than
  // the page contents. The view calls this function when the tab is focused
  // to see what it should do.
  virtual bool FocusLocationBarByDefault() = 0;

  // Does this have an opener associated with it?
  virtual bool HasOpener() const = 0;

  typedef base::Callback<void(int, /* id */
                              int, /* HTTP status code */
                              const GURL&, /* image_url */
                              int,  /* requested_size */
                              const std::vector<SkBitmap>& /* bitmaps*/)>
      ImageDownloadCallback;

  // Sends a request to download the given image |url| and returns the unique
  // id of the download request. When the download is finished, |callback| will
  // be called with the bitmaps received from the renderer. If |is_favicon| is
  // true, the cookies are not sent and not accepted during download. Note that
  // |preferred_image_size| is a hint for images with multiple sizes. The
  // downloaded image is not resized to the given image_size. If 0 is passed,
  // the first frame of the image is returned.
  // |max_image_size| is the maximal size of the returned image. It will be
  // resized if needed. If 0 is passed, the maximal size is unlimited.
  virtual int DownloadImage(const GURL& url,
                            bool is_favicon,
                            uint32_t preferred_image_size,
                            uint32_t max_image_size,
                            const ImageDownloadCallback& callback) = 0;

 private:
  // This interface should only be implemented inside content.
  friend class WebContentsImpl;
  WebContents() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_H_
