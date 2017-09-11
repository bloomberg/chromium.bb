// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_WEB_CONTENTS_MANAGER_H_
#define CHROMECAST_BROWSER_CAST_WEB_CONTENTS_MANAGER_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chromecast/browser/cast_web_view.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace chromecast {

// This class dispenses CastWebView objects which are used to wrap WebContents
// in cast_shell. This class can take ownership of a WebContents instance when
// the page is in the process of tearing down; we cannot simply post a delayed
// task since WebContents may try to use browser objects that get deleted as a
// result of browser shutdown.
class CastWebContentsManager {
 public:
  explicit CastWebContentsManager(content::BrowserContext* browser_context);
  ~CastWebContentsManager();

  std::unique_ptr<CastWebView> CreateWebView(
      CastWebView::Delegate* delegate,
      scoped_refptr<content::SiteInstance> site_instance,
      bool transparent,
      bool allow_media_access);

  // Take ownership of |web_contents| and delete after |time_delta|, or sooner
  // if necessary.
  void DelayWebContentsDeletion(
      std::unique_ptr<content::WebContents> web_contents,
      base::TimeDelta time_delta);

 private:
  void DeleteWebContents(content::WebContents* web_contents);

  content::BrowserContext* const browser_context_;
  base::flat_set<std::unique_ptr<content::WebContents>> expiring_web_contents_;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CastWebContentsManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastWebContentsManager);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_WEB_CONTENTS_MANAGER_H_
