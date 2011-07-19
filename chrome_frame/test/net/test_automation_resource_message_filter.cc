// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test/net/test_automation_resource_message_filter.h"

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
    RequestMap::iterator it = requests_.find(request_id);
    if (it != requests_.end()) {
      handled = true;
      IPC::Message* msg = new IPC::Message(message);
      RequestJob& job = it->second;
      job.loop_->PostTask(FROM_HERE, NewRunnableFunction(OnRequestMessage,
                                                         job.job_, msg));
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
  DCHECK(requests_.end() == requests_.find(job->id()));
  RequestJob request_job = { MessageLoop::current(), job };
  requests_[job->id()] = request_job;
  return true;
}

// Remove request from the list of outstanding requests.
void TestAutomationResourceMessageFilter::UnRegisterRequest(
    URLRequestAutomationJob* job) {
  requests_.erase(job->id());
}
