// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_H_
#define IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "ios/web/public/referrer.h"
#include "ios/web/public/web_state/url_verification_constants.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

class GURL;
class SkBitmap;

@class CRWJSInjectionReceiver;
@protocol CRWScrollableContent;
@protocol CRWWebViewProxy;
typedef id<CRWWebViewProxy> CRWWebViewProxyType;
@class UIView;
typedef UIView<CRWScrollableContent> CRWContentView;

namespace base {
class DictionaryValue;
class Value;
}

namespace web {

class BrowserState;
class NavigationManager;
class WebInterstitial;
class WebStateDelegate;
class WebStateObserver;
class WebStatePolicyDecider;
class WebStateWeakPtrFactory;

// Core interface for interaction with the web.
class WebState : public base::SupportsUserData {
 public:
  // Parameters for the OpenURL() method.
  struct OpenURLParams {
    OpenURLParams(const GURL& url,
                  const Referrer& referrer,
                  WindowOpenDisposition disposition,
                  ui::PageTransition transition,
                  bool is_renderer_initiated);
    ~OpenURLParams();

    // The URL/referrer to be opened.
    GURL url;
    Referrer referrer;

    // The disposition requested by the navigation source.
    WindowOpenDisposition disposition;

    // The transition type of navigation.
    ui::PageTransition transition;

    // Whether this navigation is initiated by the renderer process.
    bool is_renderer_initiated;
  };

  // Callback for |DownloadImage()|.
  typedef base::Callback<void(
      int, /* id */
      int, /* HTTP status code */
      const GURL&, /* image_url */
      const std::vector<SkBitmap>&, /* bitmaps */
      /* The sizes in pixel of the bitmaps before they were resized due to the
         max bitmap size passed to DownloadImage(). Each entry in the bitmaps
         vector corresponds to an entry in the sizes vector. If a bitmap was
         resized, there should be a single returned bitmap. */
      const std::vector<gfx::Size>&)>
          ImageDownloadCallback;

  ~WebState() override {}

  // Gets/Sets the delegate.
  virtual WebStateDelegate* GetDelegate() = 0;
  virtual void SetDelegate(WebStateDelegate* delegate) = 0;

  // The view containing the contents of the current web page. If the view has
  // been purged due to low memory, this will recreate it. It is up to the
  // caller to size the view.
  virtual UIView* GetView() = 0;

  // Gets the BrowserState associated with this WebState. Can never return null.
  virtual BrowserState* GetBrowserState() const = 0;

  // Opens a URL with the given disposition.  The transition specifies how this
  // navigation should be recorded in the history system (for example, typed).
  virtual void OpenURL(const OpenURLParams& params) = 0;

  // Gets the NavigationManager associated with this WebState. Can never return
  // null.
  virtual NavigationManager* GetNavigationManager() = 0;

  // Gets the CRWJSInjectionReceiver associated with this WebState.
  virtual CRWJSInjectionReceiver* GetJSInjectionReceiver() const = 0;

  // Runs JavaScript in the main frame's context. If a callback is provided, it
  // will be used to return the result, when the result is available or script
  // execution has failed due to an error.
  // NOTE: Integer values will be returned as TYPE_DOUBLE because of underlying
  // library limitation.
  typedef base::Callback<void(const base::Value*)> JavaScriptResultCallback;
  virtual void ExecuteJavaScript(const base::string16& javascript) = 0;
  virtual void ExecuteJavaScript(const base::string16& javascript,
                                 const JavaScriptResultCallback& callback) = 0;

  // Gets the contents MIME type.
  virtual const std::string& GetContentsMimeType() const = 0;

  // Gets the value of the "Content-Language" HTTP header.
  virtual const std::string& GetContentLanguageHeader() const = 0;

  // Returns true if the current page is a web view with HTML.
  virtual bool ContentIsHTML() const = 0;

  // Returns the current navigation title. This could be the title of the page
  // if it is available or the URL.
  virtual const base::string16& GetTitle() const = 0;

  // Returns true if the current page is loading.
  virtual bool IsLoading() const = 0;

  // Whether this instance is in the process of being destroyed.
  virtual bool IsBeingDestroyed() const = 0;

  // Gets the URL currently being displayed in the URL bar, if there is one.
  // This URL might be a pending navigation that hasn't committed yet, so it is
  // not guaranteed to match the current page in this WebState. A typical
  // example of this is interstitials, which show the URL of the new/loading
  // page (active) but the security context is of the old page (last committed).
  virtual const GURL& GetVisibleURL() const = 0;

  // Gets the last committed URL. It represents the current page that is
  // displayed in this WebState. It represents the current security context.
  virtual const GURL& GetLastCommittedURL() const = 0;

  // Returns the WebState view of the current URL. Moreover, this method
  // will set the trustLevel enum to the appropriate level from a security point
  // of view. The caller has to handle the case where |trust_level| is not
  // appropriate.
  // TODO(stuartmorgan): Figure out a clean API for this.
  // See http://crbug.com/457679
  virtual GURL GetCurrentURL(URLVerificationTrustLevel* trust_level) const = 0;

  // Resizes |content_view| to the content area's size and adds it to the
  // hierarchy.  A navigation will remove the view from the hierarchy.
  virtual void ShowTransientContentView(CRWContentView* content_view) = 0;

  // Returns true if a WebInterstitial is currently displayed.
  virtual bool IsShowingWebInterstitial() const = 0;

  // Returns the currently visible WebInterstitial if one is shown.
  virtual WebInterstitial* GetWebInterstitial() const = 0;

  // Returns the unique ID to use with web::CertStore.
  virtual int GetCertGroupId() const = 0;

  // Callback used to handle script commands.
  // The callback must return true if the command was handled, and false
  // otherwise.
  // In particular the callback must return false if the command is unexpected
  // or ill-formatted.
  // The first parameter is the content of the command, the second parameter is
  // the URL of the page, and the third parameter is a bool indicating if the
  // user is currently interacting with the page.
  typedef base::Callback<bool(const base::DictionaryValue&, const GURL&, bool)>
      ScriptCommandCallback;

  // Registers a callback that will be called when a command matching
  // |command_prefix| is received.
  virtual void AddScriptCommandCallback(const ScriptCommandCallback& callback,
                                        const std::string& command_prefix) = 0;

  // Removes the callback associated with |command_prefix|.
  virtual void RemoveScriptCommandCallback(
      const std::string& command_prefix) = 0;

  // Returns the current CRWWebViewProxy object.
  virtual CRWWebViewProxyType GetWebViewProxy() const = 0;

  // Sends a request to download the given image |url| and returns the unique
  // id of the download request. When the download is finished, |callback| will
  // be called with the bitmaps received from the renderer.
  // If |is_favicon| is true, the cookies are not sent and not accepted during
  // download.
  // Bitmaps with pixel sizes larger than |max_bitmap_size| are filtered out
  // from the bitmap results. If there are no bitmap results <=
  // |max_bitmap_size|, the smallest bitmap is resized to |max_bitmap_size| and
  // is the only result. A |max_bitmap_size| of 0 means unlimited.
  // If |bypass_cache| is true, |url| is requested from the server even if it
  // is present in the browser cache.
  virtual int DownloadImage(const GURL& url,
                            bool is_favicon,
                            uint32_t max_bitmap_size,
                            bool bypass_cache,
                            const ImageDownloadCallback& callback) = 0;

 protected:
  friend class WebStateObserver;
  friend class WebStatePolicyDecider;

  // Adds and removes observers for page navigation notifications. The order in
  // which notifications are sent to observers is undefined. Clients must be
  // sure to remove the observer before they go away.
  // TODO(droger): Move these methods to WebStateImpl once it is in ios/.
  virtual void AddObserver(WebStateObserver* observer) = 0;
  virtual void RemoveObserver(WebStateObserver* observer) = 0;

  // Adds and removes policy deciders for navigation actions. The order in which
  // deciders are called is undefined, and will stop on the first decider that
  // refuses a navigation. Clients must be sure to remove the deciders before
  // they go away.
  virtual void AddPolicyDecider(WebStatePolicyDecider* decider) = 0;
  virtual void RemovePolicyDecider(WebStatePolicyDecider* decider) = 0;

  WebState() {}

 private:
  friend class WebStateWeakPtrFactory;  // For AsWeakPtr.

  // Returns a WeakPtr<WebState> to the current WebState. Must remain private
  // and only call must be in WebStateWeakPtrFactory. Please consult that class
  // for more details. Remove as part of http://crbug.com/556736.
  virtual base::WeakPtr<WebState> AsWeakPtr() = 0;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_H_
