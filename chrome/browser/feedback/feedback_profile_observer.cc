// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/feedback_profile_observer.h"

#include "base/callback.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "components/feedback/feedback_report.h"
#include "components/feedback/feedback_uploader.h"
#include "components/feedback/feedback_uploader_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

using content::BrowserThread;

static base::LazyInstance<feedback::FeedbackProfileObserver>::Leaky
    g_feedback_profile_observer = LAZY_INSTANCE_INITIALIZER;

namespace feedback {

// static
void FeedbackProfileObserver::Initialize() {
  g_feedback_profile_observer.Get();
}

FeedbackProfileObserver::FeedbackProfileObserver() {
  prefs_registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CREATED,
                       content::NotificationService::AllSources());
}

FeedbackProfileObserver::~FeedbackProfileObserver() {
  prefs_registrar_.RemoveAll();
}

void FeedbackProfileObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(chrome::NOTIFICATION_PROFILE_CREATED, type);

  Profile* profile = content::Source<Profile>(source).ptr();
  if (profile && !profile->IsOffTheRecord())
    QueueUnsentReports(profile);
}

void FeedbackProfileObserver::QueueSingleReport(
    feedback::FeedbackUploader* uploader,
    const std::string& data) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::BindOnce(&FeedbackUploader::QueueReport,
                                         uploader->AsWeakPtr(), data));
}

void FeedbackProfileObserver::QueueUnsentReports(
    content::BrowserContext* context) {
  feedback::FeedbackUploader* uploader =
      feedback::FeedbackUploaderFactory::GetForBrowserContext(context);
  base::PostTaskWithTraits(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
      base::BindOnce(
          &FeedbackReport::LoadReportsAndQueue,
          uploader->GetFeedbackReportsPath(),
          base::Bind(&FeedbackProfileObserver::QueueSingleReport, uploader)));
}

}  // namespace feedback
