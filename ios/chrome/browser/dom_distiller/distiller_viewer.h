// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOM_DISTILLER_DISTILLER_VIEWER_H_
#define IOS_CHROME_BROWSER_DOM_DISTILLER_DISTILLER_VIEWER_H_

#include "base/memory/scoped_ptr.h"
#include "components/dom_distiller/core/task_tracker.h"

class GURL;

namespace ios {
class ChromeBrowserState;
}

namespace dom_distiller {

class DistilledPagePrefs;

// A very simple and naive implementation of the dom_distiller
// ViewRequestDelegate: From an URL it builds an HTML string and notifies when
// finished.
class DistillerViewer : public dom_distiller::ViewRequestDelegate {
 public:
  typedef base::Callback<void(const GURL&, const std::string&)>
      DistillationFinishedCallback;

  DistillerViewer(ios::ChromeBrowserState* browser_state,
                  const GURL& url,
                  const DistillationFinishedCallback& callback);
  ~DistillerViewer() override;

  // ViewRequestDelegate.
  void OnArticleUpdated(
      dom_distiller::ArticleDistillationUpdate article_update) override {}
  void OnArticleReady(const DistilledArticleProto* article_proto) override;

 private:
  // The url of the distilled page.
  const GURL url_;
  // Callback to invoke when the page is finished.
  DistillationFinishedCallback callback_;
  // Interface for accessing preferences for distilled pages.
  scoped_ptr<DistilledPagePrefs> distilled_page_prefs_;
  // Keeps the distiller going until the view is released.
  scoped_ptr<dom_distiller::ViewerHandle> viewer_handle_;

  DISALLOW_COPY_AND_ASSIGN(DistillerViewer);
};

}  // namespace dom_distiller

#endif  // IOS_CHROME_BROWSER_DOM_DISTILLER_DISTILLER_VIEWER_H_
