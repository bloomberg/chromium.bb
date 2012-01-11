// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test/net/test_automation_resource_message_filter.h"

#include "base/bind.h"

TestAutomationResourceMessageFilter::TestAutomationResourceMessageFilter(
    AutomationProvider* automation) : automation_(automation) {
}

bool TestAutomationResourceMessageFilter::Send(IPC::Message* message) {
  return automation_->Send(message);
}

// static
void TestAutomationResourceMessageFilter::OnRequestMessage(
    URLRequestAutomationJob* job, IPC::Message* msg) {
  job->OnMessage(*msg);
  delete msg;
}

bool TestAutomationResourceMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  // See if we want to process this message... call the base class
  // for filter messages, send the message to the correct thread
  // for URL requests.
  bool handled = false;
  int request_id;
  if (URLRequestAutomationJob::MayFilterMessage(message, &request_id)) {
    base::AutoLock lock(requests_lock_);
    RequestMap::iterator it = requests_.find(request_id);
    if (it != requests_.end()) {
      handled = true;
      IPC::Message* msg = new IPC::Message(message);
      RequestJob& job = it->second;

      // SUBTLE: Why is this safe? We pass the URLRequestAutomationJob to a
      // posted task which then takes a reference. We then release the lock,
      // meaning we are no longer protecting access to the request_map_ which
      // holds our last owned reference to the URLRequestAutomationJob. Thus
      // the posted task could be the one holding the last reference.
      //
      // If the posted task were to be run on a thread other than the one the
      // URLRequestAutomationJob was created on, we could destroy the job on
      // the wrong thread (resulting in badness as URLRequestJobs must be
      // created and destroyed on the same thread). The destruction will happen
      // on the correct thread here since we post to job.loop_ which is set as
      // the message loop of the current thread when RegisterRequest is invoked
      // by URLRequestJob's constructor.
      job.loop_->PostTask(FROM_HERE,
                          base::Bind(OnRequestMessage, job.job_, msg));
    }
  } else {
    handled = AutomationResourceMessageFilter::OnMessageReceived(message);
  }
  return handled;
}

// Add request to the list of outstanding requests.
bool TestAutomationResourceMessageFilter::RegisterRequest(
    URLRequestAutomationJob* job) {
  // Store the request in an internal map like the parent class
  // does, but also store the current loop pointer so we can send
  // request messages to that loop.
  base::AutoLock lock(requests_lock_);
  DCHECK(requests_.end() == requests_.find(job->id()));
  RequestJob request_job = { MessageLoop::current(), job };
  requests_[job->id()] = request_job;
  return true;
}

// Remove request from the list of outstanding requests.
void TestAutomationResourceMessageFilter::UnRegisterRequest(
    URLRequestAutomationJob* job) {
  base::AutoLock lock(requests_lock_);
  requests_.erase(job->id());
}
