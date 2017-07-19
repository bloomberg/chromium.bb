// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_permission_context.h"

#include <deque>

#include "base/callback.h"
#include "base/location.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "chrome/browser/notifications/desktop_notification_profile_util.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace {

// At most one of these is attached to each WebContents. It allows posting
// delayed tasks whose timer only counts down whilst the WebContents is visible
// (and whose timer is reset whenever the WebContents stops being visible).
class VisibilityTimerTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<VisibilityTimerTabHelper> {
 public:
  ~VisibilityTimerTabHelper() override {}

  // Runs |task| after the WebContents has been visible for a consecutive
  // duration of at least |visible_delay|.
  void PostTaskAfterVisibleDelay(const tracked_objects::Location& from_here,
                                 const base::Closure& task,
                                 base::TimeDelta visible_delay,
                                 const PermissionRequestID& id);

  // Deletes any earlier task(s) that match |id|.
  void CancelTask(const PermissionRequestID& id);

  // WebContentsObserver:
  void WasShown() override;
  void WasHidden() override;
  void WebContentsDestroyed() override;

 private:
  friend class content::WebContentsUserData<VisibilityTimerTabHelper>;
  explicit VisibilityTimerTabHelper(content::WebContents* contents);

  void RunTask(const base::Closure& task);

  bool is_visible_;

  struct Task {
    Task(const PermissionRequestID& id, std::unique_ptr<base::Timer> timer)
        : id(id), timer(std::move(timer)) {}

    Task& operator=(Task&& other) {
      id = other.id;
      timer = std::move(other.timer);
      return *this;
    }

    PermissionRequestID id;
    std::unique_ptr<base::Timer> timer;

   private:
    DISALLOW_COPY_AND_ASSIGN(Task);
  };
  std::deque<Task> task_queue_;

  DISALLOW_COPY_AND_ASSIGN(VisibilityTimerTabHelper);
};

VisibilityTimerTabHelper::VisibilityTimerTabHelper(
    content::WebContents* contents)
    : content::WebContentsObserver(contents) {
  if (!contents->GetMainFrame()) {
    is_visible_ = false;
  } else {
    switch (contents->GetMainFrame()->GetVisibilityState()) {
      case blink::kWebPageVisibilityStateHidden:
      case blink::kWebPageVisibilityStatePrerender:
        is_visible_ = false;
        break;
      case blink::kWebPageVisibilityStateVisible:
        is_visible_ = true;
        break;
    }
  }
}

void VisibilityTimerTabHelper::PostTaskAfterVisibleDelay(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta visible_delay,
    const PermissionRequestID& id) {
  if (web_contents()->IsBeingDestroyed())
    return;

  // Safe to use Unretained, as destroying this will destroy task_queue_, hence
  // cancelling all timers.
  std::unique_ptr<base::Timer> timer(new base::Timer(
      from_here, visible_delay, base::Bind(&VisibilityTimerTabHelper::RunTask,
                                           base::Unretained(this), task),
      false /* is_repeating */));
  DCHECK(!timer->IsRunning());

  task_queue_.emplace_back(id, std::move(timer));

  if (is_visible_ && task_queue_.size() == 1)
    task_queue_.front().timer->Reset();
}

void VisibilityTimerTabHelper::CancelTask(const PermissionRequestID& id) {
  bool deleting_front = task_queue_.front().id == id;

  base::EraseIf(task_queue_, [id](const Task& task) { return task.id == id; });

  if (!task_queue_.empty() && is_visible_ && deleting_front)
    task_queue_.front().timer->Reset();
}

void VisibilityTimerTabHelper::WasShown() {
  if (!is_visible_ && !task_queue_.empty())
    task_queue_.front().timer->Reset();
  is_visible_ = true;
}

void VisibilityTimerTabHelper::WasHidden() {
  if (is_visible_ && !task_queue_.empty())
    task_queue_.front().timer->Stop();
  is_visible_ = false;
}

void VisibilityTimerTabHelper::WebContentsDestroyed() {
  task_queue_.clear();
}

void VisibilityTimerTabHelper::RunTask(const base::Closure& task) {
  DCHECK(is_visible_);
  task.Run();
  task_queue_.pop_front();
  if (!task_queue_.empty())
    task_queue_.front().timer->Reset();
}

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(VisibilityTimerTabHelper);

NotificationPermissionContext::NotificationPermissionContext(
    Profile* profile,
    ContentSettingsType content_settings_type)
    : PermissionContextBase(profile,
                            content_settings_type,
                            blink::WebFeaturePolicyFeature::kNotFound),
      weak_factory_ui_thread_(this) {
  DCHECK(content_settings_type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS ||
         content_settings_type == CONTENT_SETTINGS_TYPE_PUSH_MESSAGING);
}

NotificationPermissionContext::~NotificationPermissionContext() {}

void NotificationPermissionContext::ResetPermission(
    const GURL& requesting_origin,
    const GURL& embedder_origin) {
  DesktopNotificationProfileUtil::ClearSetting(profile(), requesting_origin);
}

void NotificationPermissionContext::CancelPermissionRequest(
    content::WebContents* web_contents,
    const PermissionRequestID& id) {
  if (profile()->IsOffTheRecord()) {
    VisibilityTimerTabHelper::FromWebContents(web_contents)->CancelTask(id);
  } else {
    PermissionContextBase::CancelPermissionRequest(web_contents, id);
  }
}

void NotificationPermissionContext::DecidePermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    const BrowserPermissionCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Permission requests for either Web Notifications and Push Notifications may
  // only happen on top-level frames. Usage will continue to be allowed in
  // iframes: such frames could trivially work around the restriction by posting
  // a message to their Service Worker, where showing a notification is allowed.
  if (requesting_origin != embedding_origin) {
    callback.Run(CONTENT_SETTING_BLOCK);
    return;
  }

  // Notifications permission is always denied in incognito. To prevent sites
  // from using that to detect whether incognito mode is active, we deny after a
  // random time delay, to simulate a user clicking a bubble/infobar. See also
  // ContentSettingsRegistry::Init, which marks notifications as
  // INHERIT_IF_LESS_PERMISSIVE, and
  // PermissionMenuModel::PermissionMenuModel which prevents users from manually
  // allowing the permission.
  if (profile()->IsOffTheRecord()) {
    // Random number of seconds in the range [1.0, 2.0).
    double delay_seconds = 1.0 + 1.0 * base::RandDouble();
    VisibilityTimerTabHelper::CreateForWebContents(web_contents);
    VisibilityTimerTabHelper::FromWebContents(web_contents)
        ->PostTaskAfterVisibleDelay(
            FROM_HERE,
            base::Bind(&NotificationPermissionContext::NotifyPermissionSet,
                       weak_factory_ui_thread_.GetWeakPtr(), id,
                       requesting_origin, embedding_origin, callback,
                       true /* persist */, CONTENT_SETTING_BLOCK),
            base::TimeDelta::FromSecondsD(delay_seconds), id);
    return;
  }

  PermissionContextBase::DecidePermission(web_contents, id, requesting_origin,
                                          embedding_origin, user_gesture,
                                          callback);
}

// Unlike other permission types, granting a notification for a given origin
// will not take into account the |embedder_origin|, it will only be based
// on the requesting iframe origin.
// TODO(mukai) Consider why notifications behave differently than
// other permissions. https://crbug.com/416894
void NotificationPermissionContext::UpdateContentSetting(
    const GURL& requesting_origin,
    const GURL& embedder_origin,
    ContentSetting content_setting) {
  DCHECK(content_setting == CONTENT_SETTING_ALLOW ||
         content_setting == CONTENT_SETTING_BLOCK);

  if (content_setting == CONTENT_SETTING_ALLOW) {
    DesktopNotificationProfileUtil::GrantPermission(profile(),
                                                    requesting_origin);
  } else {
    DesktopNotificationProfileUtil::DenyPermission(profile(),
                                                   requesting_origin);
  }
}

bool NotificationPermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}
