// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_data_experimental.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model_experimental.h"
#include "content/public/browser/web_contents.h"

class TabDataExperimental::ContentsWatcher
    : public content::WebContentsObserver {
 public:
  ContentsWatcher(TabStripModelExperimental* model, content::WebContents* wc)
      : WebContentsObserver(wc), tab_strip_model_(model), contents_(wc) {}

  void ReplaceContents(content::WebContents* new_contents) {
    contents_ = new_contents;
    // This tells the WebContentsObserver to watch a new contents.
    Observe(new_contents);
  }

 private:
  // WebContentsObserver implementation:
  void WebContentsDestroyed() override {
    // Note that we only detach the contents here, not close it - it's
    // already been closed. We just want to undo our bookkeeping.
    tab_strip_model_->DetachWebContents(contents_);
  }

  TabStripModelExperimental* tab_strip_model_;
  content::WebContents* contents_;

  DISALLOW_COPY_AND_ASSIGN(ContentsWatcher);
};

TabDataExperimental::TabDataExperimental() = default;
TabDataExperimental::TabDataExperimental(TabDataExperimental&&) noexcept =
    default;

TabDataExperimental::TabDataExperimental(TabDataExperimental* parent,
                                         Type type,
                                         content::WebContents* contents,
                                         TabStripModelExperimental* model)
    : parent_(parent),
      type_(type),
      contents_(contents),
      contents_watcher_(base::MakeUnique<ContentsWatcher>(model, contents)) {
  // kSingle and kHubAndSpoke requires a type.
  DCHECK(type == Type::kGroup || contents);

  // kGroup can't have a type.
  DCHECK(type != Type::kGroup || !contents);
}

TabDataExperimental::~TabDataExperimental() = default;

TabDataExperimental& TabDataExperimental::operator=(TabDataExperimental&&) =
    default;

const base::string16& TabDataExperimental::GetTitle() const {
  DCHECK(type() == Type::kSingle);
  // TODO(brettw) this will need to use TabUIHelper.
  return contents_->GetTitle();
}

bool TabDataExperimental::CountsAsViewIndex() const {
  // Don't allow selection of a group when it's expanded (then it should
  // go to the first child). Any non-group or non-expanded thing counts
  // as the view index.
  return type_ != Type::kGroup || !expanded_;
}

void TabDataExperimental::ReplaceContents(content::WebContents* new_contents) {
  DCHECK(type() == Type::kSingle);
  contents_watcher_->ReplaceContents(new_contents);
}
