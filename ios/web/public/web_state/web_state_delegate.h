// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_DELEGATE_H_
#define IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_DELEGATE_H_

#include <set>

#include "base/callback.h"

namespace net {
class SSLInfo;
}

namespace web {

struct ContextMenuParams;
struct SSLStatus;
class WebState;

// Objects implement this interface to get notified about changes in the
// WebState and to provide necessary functionality.
class WebStateDelegate {
 public:
  WebStateDelegate();

  // Notifies the delegate that the page has made some progress loading.
  // |progress| is a value between 0.0 (nothing loaded) to 1.0 (page fully
  // loaded).
  virtual void LoadProgressChanged(WebState* source, double progress);

  // Notifies the delegate that the user triggered the context menu with the
  // given |ContextMenuParams|. Returns true if the context menu operation was
  // handled by the delegate.
  virtual bool HandleContextMenu(WebState* source,
                                 const ContextMenuParams& params);

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
