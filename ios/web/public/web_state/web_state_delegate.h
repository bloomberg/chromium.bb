// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_DELEGATE_H_
#define IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_DELEGATE_H_

#include <set>

#import <Foundation/Foundation.h>

#include "base/callback.h"
#import "ios/web/public/web_state/web_state.h"

namespace web {

struct ContextMenuParams;
class JavaScriptDialogPresenter;

// Objects implement this interface to get notified about changes in the
// WebState and to provide necessary functionality.
class WebStateDelegate {
 public:
  WebStateDelegate();

  // Returns the WebState the URL is opened in, or nullptr if the URL wasn't
  // opened immediately.
  virtual WebState* OpenURLFromWebState(WebState*,
                                        const WebState::OpenURLParams&);

  // Notifies the delegate that the user triggered the context menu with the
  // given |ContextMenuParams|. Returns true if the context menu operation was
  // handled by the delegate.
  virtual bool HandleContextMenu(WebState* source,
                                 const ContextMenuParams& params);

  // Requests the repost form confirmation dialog. Clients must call |callback|
  // with true to allow repost and with false to cancel the repost. If this
  // method is not implemented then WebState will repost the form.
  virtual void ShowRepostFormWarningDialog(
      WebState* source,
      const base::Callback<void(bool)>& callback);

  // Returns a pointer to a service to manage dialogs. May return nullptr in
  // which case dialogs aren't shown.
  // TODO(crbug.com/622084): Find better place for this method.
  virtual JavaScriptDialogPresenter* GetJavaScriptDialogPresenter(
      WebState* source);

  // Called when a request receives an authentication challenge specified by
  // |protection_space|, and is unable to respond using cached credentials.
  // Clients must call |callback| even if they want to cancel authentication
  // (in which case |username| or |password| should be nil).
  typedef base::Callback<void(NSString* username, NSString* password)>
      AuthCallback;
  virtual void OnAuthRequired(WebState* source,
                              NSURLProtectionSpace* protection_space,
                              NSURLCredential* proposed_credential,
                              const AuthCallback& callback) = 0;

 protected:
  virtual ~WebStateDelegate();

 private:
  friend class WebStateImpl;

  // Called when |this| becomes the WebStateDelegate for |source|.
  void Attach(WebState* source);

  // Called when |this| is no longer the WebStateDelegate for |source|.
  void Detach(WebState* source);

  // The WebStates for which |this| is currently a delegate.
  std::set<WebState*> attached_states_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_DELEGATE_H_
