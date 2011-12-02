// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_RENDER_VIEW_OBSERVER_H_
#define CHROME_RENDERER_CHROME_RENDER_VIEW_OBSERVER_H_
#pragma once

#include <set>
#include <string>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/renderer/render_view_observer.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPermissionClient.h"

class ChromeRenderProcessObserver;
class ContentSettingsObserver;
class DomAutomationController;
class ExtensionDispatcher;
class ExternalHostBindings;
class SkBitmap;
class TranslateHelper;
struct ThumbnailScore;
class WebViewColorOverlay;

namespace WebKit {
class WebView;
}

namespace safe_browsing {
class PhishingClassifierDelegate;
}

namespace webkit_glue {
class ImageResourceFetcher;
}

// This class holds the Chrome specific parts of RenderView, and has the same
// lifetime.
class ChromeRenderViewObserver : public content::RenderViewObserver,
                                 public WebKit::WebPermissionClient {
 public:
  // translate_helper can be NULL.
  ChromeRenderViewObserver(
      content::RenderView* render_view,
      ContentSettingsObserver* content_settings,
      ChromeRenderProcessObserver* chrome_render_process_observer,
      ExtensionDispatcher* extension_dispatcher,
      TranslateHelper* translate_helper);
  virtual ~ChromeRenderViewObserver();

 private:
  // Holds the information received in OnWebUIJavaScript for later use
  // to call EvaluateScript() to preload javascript for WebUI tests.
  struct WebUIJavaScript {
    string16 frame_xpath;
    string16 jscript;
    int id;
    bool notify_result;
  };

  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidStartLoading() OVERRIDE;
  virtual void DidStopLoading() OVERRIDE;
  virtual void DidChangeIcon(WebKit::WebFrame* frame,
                             WebKit::WebIconURL::Type icon_type) OVERRIDE;
  virtual void DidCommitProvisionalLoad(WebKit::WebFrame* frame,
                                        bool is_new_navigation) OVERRIDE;
  virtual void DidClearWindowObject(WebKit::WebFrame* frame) OVERRIDE;
  virtual void DidHandleTouchEvent(const WebKit::WebTouchEvent& event) OVERRIDE;

  // WebKit::WebPermissionClient implementation.
  virtual bool allowDatabase(WebKit::WebFrame* frame,
                             const WebKit::WebString& name,
                             const WebKit::WebString& display_name,
                             unsigned long estimated_size) OVERRIDE;
  virtual bool allowFileSystem(WebKit::WebFrame* frame) OVERRIDE;
  virtual bool allowImage(WebKit::WebFrame* frame,
                          bool enabled_per_settings,
                          const WebKit::WebURL& image_url) OVERRIDE;
  virtual bool allowIndexedDB(WebKit::WebFrame* frame,
                              const WebKit::WebString& name,
                              const WebKit::WebSecurityOrigin& origin) OVERRIDE;
  virtual bool allowPlugins(WebKit::WebFrame* frame,
                            bool enabled_per_settings) OVERRIDE;
  virtual bool allowScript(WebKit::WebFrame* frame,
                           bool enabled_per_settings) OVERRIDE;
  virtual bool allowScriptFromSource(WebKit::WebFrame* frame,
                                     bool enabled_per_settings,
                                     const WebKit::WebURL& script_url) OVERRIDE;
  virtual bool allowScriptExtension(WebKit::WebFrame* frame,
                                    const WebKit::WebString& extension_name,
                                    int extension_group) OVERRIDE;
  virtual bool allowStorage(WebKit::WebFrame* frame, bool local) OVERRIDE;
  virtual bool allowReadFromClipboard(WebKit::WebFrame* frame,
                                      bool default_value) OVERRIDE;
  virtual bool allowWriteToClipboard(WebKit::WebFrame* frame,
                                     bool default_value) OVERRIDE;
  virtual void didNotAllowPlugins(WebKit::WebFrame* frame) OVERRIDE;
  virtual void didNotAllowScript(WebKit::WebFrame* frame) OVERRIDE;
  virtual bool allowDisplayingInsecureContent(
      WebKit::WebFrame* frame,
      bool allowed_per_settings,
      const WebKit::WebSecurityOrigin& context,
      const WebKit::WebURL& url) OVERRIDE;
  virtual bool allowRunningInsecureContent(
      WebKit::WebFrame* frame,
      bool allowed_per_settings,
      const WebKit::WebSecurityOrigin& context,
      const WebKit::WebURL& url) OVERRIDE;
  virtual void Navigate(const GURL& url) OVERRIDE;

  void OnWebUIJavaScript(const string16& frame_xpath,
                         const string16& jscript,
                         int id,
                         bool notify_result);
  void OnCaptureSnapshot();
  void OnHandleMessageFromExternalHost(const std::string& message,
                                       const std::string& origin,
                                       const std::string& target);
  void OnJavaScriptStressTestControl(int cmd, int param);
  void OnDownloadFavicon(int id, const GURL& image_url, int image_size);
  void OnSetIsPrerendering(bool is_prerendering);
  void OnSetAllowDisplayingInsecureContent(bool allow);
  void OnSetAllowRunningInsecureContent(bool allow);
  void OnSetClientSidePhishingDetection(bool enable_phishing_detection);
  void OnSetVisuallyDeemphasized(bool deemphasized);
  void OnStartFrameSniffer(const string16& frame_name);
  void OnGetFPS();
  void OnAddStrictSecurityHost(const std::string& host);

  // Captures the thumbnail and text contents for indexing for the given load
  // ID. If the view's load ID is different than the parameter, this call is
  // a NOP. Typically called on a timer, so the load ID may have changed in the
  // meantime.
  void CapturePageInfo(int load_id, bool preliminary_capture);

  // Retrieves the text from the given frame contents, the page text up to the
  // maximum amount kMaxIndexChars will be placed into the given buffer.
  void CaptureText(WebKit::WebFrame* frame, string16* contents);

  void CaptureThumbnail();

  // Creates a thumbnail of |frame|'s contents resized to (|w|, |h|)
  // and puts that in |thumbnail|. Thumbnail metadata goes in |score|.
  bool CaptureFrameThumbnail(WebKit::WebView* view, int w, int h,
                             SkBitmap* thumbnail,
                             ThumbnailScore* score);

  // Capture a snapshot of a view.  This is used to allow an extension
  // to get a snapshot of a tab using chrome.tabs.captureVisibleTab().
  bool CaptureSnapshot(WebKit::WebView* view, SkBitmap* snapshot);

  // Exposes the DOMAutomationController object that allows JS to send
  // information to the browser process.
  void BindDOMAutomationController(WebKit::WebFrame* webframe);

  ExternalHostBindings* GetExternalHostBindings();

  // This callback is triggered when DownloadFavicon completes, either
  // succesfully or with a failure. See DownloadFavicon for more
  // details.
  void DidDownloadFavicon(webkit_glue::ImageResourceFetcher* fetcher,
                          const SkBitmap& image);

  // Requests to download a favicon image. When done, the RenderView
  // is notified by way of DidDownloadFavicon. Returns true if the
  // request was successfully started, false otherwise. id is used to
  // uniquely identify the request and passed back to the
  // DidDownloadFavicon method. If the image has multiple frames, the
  // frame whose size is image_size is returned. If the image doesn't
  // have a frame at the specified size, the first is returned.
  bool DownloadFavicon(int id, const GURL& image_url, int image_size);

  // Decodes a data: URL image or returns an empty image in case of failure.
  SkBitmap ImageFromDataUrl(const GURL&) const;

  // Determines if a host is in the strict security host set.
  bool IsStrictSecurityHost(const std::string& host);

  // Save the JavaScript to preload if a ViewMsg_WebUIJavaScript is received.
  scoped_ptr<WebUIJavaScript> webui_javascript_;

  // Owned by ChromeContentRendererClient and outlive us.
  ChromeRenderProcessObserver* chrome_render_process_observer_;
  ExtensionDispatcher* extension_dispatcher_;

  // Have the same lifetime as us.
  ContentSettingsObserver* content_settings_;
  TranslateHelper* translate_helper_;
  safe_browsing::PhishingClassifierDelegate* phishing_classifier_;

  // Page_id from the last page we indexed. This prevents us from indexing the
  // same page twice in a row.
  int32 last_indexed_page_id_;
  // The toplevel URL that was last indexed.  This is used together with the
  // page id to decide whether to reindex in certain cases like history
  // replacement.
  GURL last_indexed_url_;

  // Insecure content may be permitted for the duration of this render view.
  bool allow_displaying_insecure_content_;
  bool allow_running_insecure_content_;
  std::set<std::string> strict_security_hosts_;

  // Allows JS to access DOM automation. The JS object is only exposed when the
  // DOM automation bindings are enabled.
  scoped_ptr<DomAutomationController> dom_automation_controller_;

  // External host exposed through automation controller.
  scoped_ptr<ExternalHostBindings> external_host_bindings_;

  base::WeakPtrFactory<ChromeRenderViewObserver> weak_factory_;

  typedef std::vector<linked_ptr<webkit_glue::ImageResourceFetcher> >
      ImageResourceFetcherList;

  // ImageResourceFetchers schedule via DownloadImage.
  ImageResourceFetcherList image_fetchers_;

  // A color page overlay when visually de-emaphasized.
  scoped_ptr<WebViewColorOverlay> dimmed_color_overlay_;

  DISALLOW_COPY_AND_ASSIGN(ChromeRenderViewObserver);
};

#endif  // CHROME_RENDERER_CHROME_RENDER_VIEW_OBSERVER_H_
