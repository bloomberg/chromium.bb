// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_data_experimental.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"

class TabDataExperimental::ContentsWatcher
    : public content::WebContentsObserver {
 public:
  ContentsWatcher(TabStripModel* strip, content::WebContents* wc)
      : WebContentsObserver(wc), tab_strip_model_(strip), contents_(wc) {}

  void ReplaceContents(content::WebContents* new_contents) {
    contents_ = new_contents;
    // This tells the WebContentsObserver to watch a new contents.
    Observe(new_contents);
  }

 private:
  // WebContentsObserver implementation:
  void WebContentsDestroyed() override {
    DCHECK_EQ(contents_, web_contents());

    // Note that we only detach the contents here, not close it - it's
    // already been closed. We just want to undo our bookkeeping.
    int index = tab_strip_model_->GetIndexOfWebContents(contents_);
    DCHECK_NE(TabStripModel::kNoTab, index);
    tab_strip_model_->DetachWebContentsAt(index);
  }

  TabStripModel* tab_strip_model_;
  content::WebContents* contents_;

  DISALLOW_COPY_AND_ASSIGN(ContentsWatcher);
};

TabDataExperimental::TabDataExperimental() = default;
TabDataExperimental::TabDataExperimental(TabDataExperimental&&) noexcept =
    default;

TabDataExperimental::TabDataExperimental(content::WebContents* contents,
                                         TabStripModel* model)
    : contents_(contents),
      contents_watcher_(base::MakeUnique<ContentsWatcher>(model, contents)) {}

TabDataExperimental::~TabDataExperimental() = default;

TabDataExperimental& TabDataExperimental::operator=(TabDataExperimental&&) =
    default;

const base::string16& TabDataExperimental::GetTitle() const {
  DCHECK(type() == Type::kSingle);
  // TODO(brettw) this will need to use TabUIHelper.
  return contents_->GetTitle();
}

void TabDataExperimental::ReplaceContents(content::WebContents* new_contents) {
  DCHECK(type() == Type::kSingle);
  contents_watcher_->ReplaceContents(new_contents);
}
