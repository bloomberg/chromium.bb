// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/providers/web_contents/renderer_task.h"

#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/favicon/favicon_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

namespace task_management {

namespace {

// Gets the profile name associated with the browser context of the given
// |render_process_host| from the profile info cache.
base::string16 GetRendererProfileName(
    const content::RenderProcessHost* render_process_host) {
  Profile* profile =
      Profile::FromBrowserContext(render_process_host->GetBrowserContext());
  DCHECK(profile);
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t index =
      cache.GetIndexOfProfileWithPath(profile->GetOriginalProfile()->GetPath());
  if (index != std::string::npos)
    return cache.GetNameOfProfileAtIndex(index);

  return base::string16();
}

}  // namespace

RendererTask::RendererTask(const base::string16& title,
                           const gfx::ImageSkia* icon,
                           content::WebContents* web_contents)
    : Task(title, icon, web_contents->GetRenderProcessHost()->GetHandle()),
      web_contents_(web_contents),
      render_process_host_(web_contents->GetRenderProcessHost()),
      render_process_id_(render_process_host_->GetID()),
      v8_memory_allocated_(0),
      v8_memory_used_(0),
      webcache_stats_(),
      profile_name_(GetRendererProfileName(render_process_host_)) {
}

RendererTask::~RendererTask() {
}

void RendererTask::Activate() {
  if (!web_contents_->GetDelegate())
    return;

  web_contents_->GetDelegate()->ActivateContents(web_contents_);
}

void RendererTask::Refresh(const base::TimeDelta& update_interval) {
  Task::Refresh(update_interval);

  // TODO(afakhry):
  // 1- Add code to disable this refresh if it was never requested by clients of
  //    the task manager.
  // 2- Figure out a way to refresh V8 and WebCache stats cleanly either by
  //    working with amistry, or by implementing something myself.
}

Task::Type RendererTask::GetType() const {
  return Task::RENDERER;
}

int RendererTask::GetChildProcessUniqueID() const {
  return render_process_id_;
}

base::string16 RendererTask::GetProfileName() const {
  return profile_name_;
}

int64 RendererTask::GetV8MemoryAllocated() const {
  return v8_memory_allocated_;
}

int64 RendererTask::GetV8MemoryUsed() const {
  return v8_memory_used_;
}

bool RendererTask::ReportsWebCacheStats() const {
  return true;
}

blink::WebCache::ResourceTypeStats RendererTask::GetWebCacheStats() const {
  return webcache_stats_;
}

// static
base::string16 RendererTask::GetTitleFromWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  base::string16 title = web_contents->GetTitle();
  if (title.empty()) {
    GURL url = web_contents->GetURL();
    title = base::UTF8ToUTF16(url.spec());
    // Force URL to be LTR.
    title = base::i18n::GetDisplayStringInLTRDirectionality(title);
  } else {
    // Since the title could later be concatenated with
    // IDS_TASK_MANAGER_TAB_PREFIX (for example), we need to explicitly set the
    // title to be LTR format if there is no strong RTL charater in it.
    // Otherwise, if IDS_TASK_MANAGER_TAB_PREFIX is an RTL word, the
    // concatenated result might be wrong. For example, http://mail.yahoo.com,
    // whose title is "Yahoo! Mail: The best web-based Email!", without setting
    // it explicitly as LTR format, the concatenated result will be "!Yahoo!
    // Mail: The best web-based Email :BAT", in which the capital letters "BAT"
    // stands for the Hebrew word for "tab".
    base::i18n::AdjustStringForLocaleDirection(&title);
  }
  return title;
}

// static
const gfx::ImageSkia* RendererTask::GetFaviconFromWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);

  // Tag the web_contents with a |ContentFaviconDriver| (if needed) so that
  // we can use it to retrieve the favicon if there is one.
  favicon::CreateContentFaviconDriverForWebContents(web_contents);
  gfx::Image image =
      favicon::ContentFaviconDriver::FromWebContents(web_contents)->
          GetFavicon();
  if (image.IsEmpty())
    return nullptr;

  return image.ToImageSkia();
}

}  // namespace task_management
