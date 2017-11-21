// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/web_contents_closer.h"

#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace {

// CloseTracker is used when closing a set of WebContents. It listens for
// deletions of the WebContents and removes from the internal set any time one
// is deleted.
class CloseTracker {
 public:
  using Contents = std::vector<content::WebContents*>;

  explicit CloseTracker(const Contents& contents);
  ~CloseTracker();

  // Returns true if there is another WebContents in the Tracker.
  bool HasNext() const;

  // Returns the next WebContents, or NULL if there are no more.
  content::WebContents* Next();

 private:
  class DeletionObserver : public content::WebContentsObserver {
   public:
    DeletionObserver(CloseTracker* parent, content::WebContents* web_contents)
        : WebContentsObserver(web_contents), parent_(parent) {}

   private:
    // WebContentsObserver:
    void WebContentsDestroyed() override {
      parent_->OnWebContentsDestroyed(this);
    }

    CloseTracker* parent_;

    DISALLOW_COPY_AND_ASSIGN(DeletionObserver);
  };

  void OnWebContentsDestroyed(DeletionObserver* observer);

  using Observers = std::vector<std::unique_ptr<DeletionObserver>>;
  Observers observers_;

  DISALLOW_COPY_AND_ASSIGN(CloseTracker);
};

CloseTracker::CloseTracker(const Contents& contents) {
  observers_.reserve(contents.size());
  for (content::WebContents* current : contents)
    observers_.push_back(base::MakeUnique<DeletionObserver>(this, current));
}

CloseTracker::~CloseTracker() {
  DCHECK(observers_.empty());
}

bool CloseTracker::HasNext() const {
  return !observers_.empty();
}

content::WebContents* CloseTracker::Next() {
  if (observers_.empty())
    return nullptr;

  DeletionObserver* observer = observers_[0].get();
  content::WebContents* web_contents = observer->web_contents();
  observers_.erase(observers_.begin());
  return web_contents;
}

void CloseTracker::OnWebContentsDestroyed(DeletionObserver* observer) {
  for (auto i = observers_.begin(); i != observers_.end(); ++i) {
    if (observer == i->get()) {
      observers_.erase(i);
      return;
    }
  }
  NOTREACHED() << "WebContents destroyed that wasn't in the list";
}

}  // namespace

bool CloseWebContentses(WebContentsCloseDelegate* delegate,
                        const std::vector<content::WebContents*>& items,
                        uint32_t close_types) {
  if (items.empty())
    return true;

  CloseTracker close_tracker(items);

  // We only try the fast shutdown path if the whole browser process is *not*
  // shutting down. Fast shutdown during browser termination is handled in
  // browser_shutdown::OnShutdownStarting.
  if (browser_shutdown::GetShutdownType() == browser_shutdown::NOT_VALID) {
    // Construct a map of processes to the number of associated tabs that are
    // closing.
    base::flat_map<content::RenderProcessHost*, size_t> processes;
    for (content::WebContents* contents : items) {
      if (delegate->ShouldRunUnloadListenerBeforeClosing(contents))
        continue;
      content::RenderProcessHost* process =
          contents->GetMainFrame()->GetProcess();
      ++processes[process];
    }

    // Try to fast shutdown the tabs that can close.
    for (const auto& pair : processes)
      pair.first->FastShutdownIfPossible(pair.second, false);
  }

  // We now return to our regularly scheduled shutdown procedure.
  bool closed_all = true;
  while (close_tracker.HasNext()) {
    content::WebContents* closing_contents = close_tracker.Next();
    if (!delegate->ContainsWebContents(closing_contents))
      continue;

    CoreTabHelper* core_tab_helper =
        CoreTabHelper::FromWebContents(closing_contents);
    core_tab_helper->OnCloseStarted();

    // Update the explicitly closed state. If the unload handlers cancel the
    // close the state is reset in Browser. We don't update the explicitly
    // closed state if already marked as explicitly closed as unload handlers
    // call back to this if the close is allowed.
    if (!closing_contents->GetClosedByUserGesture()) {
      closing_contents->SetClosedByUserGesture(
          close_types & TabStripModel::CLOSE_USER_GESTURE);
    }

    if (delegate->RunUnloadListenerBeforeClosing(closing_contents)) {
      closed_all = false;
      continue;
    }

    delegate->OnWillDeleteWebContents(closing_contents, close_types);

    delete closing_contents;
  }

  return closed_all;
}
