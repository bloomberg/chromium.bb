// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOM_DISTILLER_DISTILLER_VIEWER_H_
#define IOS_CHROME_BROWSER_DOM_DISTILLER_DISTILLER_VIEWER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/dom_distiller/core/dom_distiller_request_view_base.h"
#include "components/dom_distiller/core/task_tracker.h"

class GURL;

namespace dom_distiller {

class DistilledPagePrefs;

// An interface for a dom_distiller ViewRequestDelegate that distills a URL and
// calls the given callback with the distilled HTML string and the images it
// contains.
class DistillerViewerInterface : public DomDistillerRequestViewBase {
 public:
  struct ImageInfo {
    // The url of the image.
    GURL url;
    // The image data as a string.
    std::string data;
  };
  typedef base::Callback<void(const GURL& url,
                              const std::string& html,
                              const std::vector<ImageInfo>& images,
                              const std::string& title)>
      DistillationFinishedCallback;

  DistillerViewerInterface(dom_distiller::DomDistillerService* distillerService,
                           PrefService* prefs)
      : DomDistillerRequestViewBase(new DistilledPagePrefs(prefs)) {}
  ~DistillerViewerInterface() override {}

  void OnArticleReady(
      const dom_distiller::DistilledArticleProto* article_proto) override = 0;

  void SendJavaScript(const std::string& buffer) override = 0;

  DISALLOW_COPY_AND_ASSIGN(DistillerViewerInterface);
};

// A very simple and naive implementation of the DistillerViewer.
class DistillerViewer : public DistillerViewerInterface {
 public:
  // Creates a |DistillerView| that will be used to distill |url|.
  // If |factory| is not null, it will be used to create the DistillerPage that
  // will load the URL and distill the page. If |factory| is null, the default
  // factory of |distillerService| will be used.
  // |callback| is called when distillation is finished with the protobuf
  // containing the distilled page.
  DistillerViewer(dom_distiller::DomDistillerService* distillerService,
                  PrefService* prefs,
                  const GURL& url,
                  const DistillationFinishedCallback& callback,
                  const DistillerPageFactory* factory);
  ~DistillerViewer() override;

  void OnArticleReady(
      const dom_distiller::DistilledArticleProto* article_proto) override;

  void SendJavaScript(const std::string& buffer) override;

 private:
  // The url of the distilled page.
  const GURL url_;
  // JavaScript buffer.
  std::string js_buffer_;
  // Callback to run once distillation is complete.
  const DistillationFinishedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(DistillerViewer);
};

}  // namespace dom_distiller

#endif  // IOS_CHROME_BROWSER_DOM_DISTILLER_DISTILLER_VIEWER_H_
