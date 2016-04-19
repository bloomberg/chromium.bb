// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_WEB_CONTENTS_INFORMATION_H_
#define CHROME_BROWSER_TASK_MANAGER_WEB_CONTENTS_INFORMATION_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content {
class WebContents;
}

namespace task_manager {

class RendererResource;

// This interface gives a WebContentsResourceProvider information about a
// category of WebContentses owned by some service. The instances are expected
// to be owned by their WebContentsResourceProvider, which will track and adapt
// the WebContentses into TaskManager resources.
class WebContentsInformation {
 public:
  typedef base::Callback<void(content::WebContents*)> NewWebContentsCallback;

  WebContentsInformation();
  virtual ~WebContentsInformation();

  // Retrieve all known WebContents instances from the service we're tracking.
  // Invoke |callback| on each one.
  virtual void GetAll(const NewWebContentsCallback& callback) = 0;

  // Return true if |web_contents| is from the service we're tracking.
  virtual bool CheckOwnership(content::WebContents* web_contents) = 0;

  // Start listening to the creation of new WebContents instances from
  // the service. While listening, invoke |callback| on each new instance.
  virtual void StartObservingCreation(
      const NewWebContentsCallback& callback) = 0;

  // Stop listening to creation of new WebContents instances.
  virtual void StopObservingCreation() = 0;

  // Create a new task manager resource for the given WebContents instance.
  virtual std::unique_ptr<RendererResource> MakeResource(
      content::WebContents* web_contents) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebContentsInformation);
};

// Implements the observer methods (StartObservingCreation() /
// StopObservingCreation()) of WebContentsInformation using
// NOTIFICATION_WEB_CONTENTS_CONNECTED.
class NotificationObservingWebContentsInformation
    : public WebContentsInformation,
      public content::NotificationObserver {
 public:
  NotificationObservingWebContentsInformation();
  ~NotificationObservingWebContentsInformation() override;

  // WebContentsInformation partial implementation.
  void StartObservingCreation(const NewWebContentsCallback& callback) override;
  void StopObservingCreation() override;

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  content::NotificationRegistrar registrar_;
  NewWebContentsCallback observer_callback_;
  DISALLOW_COPY_AND_ASSIGN(NotificationObservingWebContentsInformation);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_WEB_CONTENTS_INFORMATION_H_
