// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PEPPER_GTALK_MESSAGE_FILTER_H_
#define CHROME_BROWSER_PEPPER_GTALK_MESSAGE_FILTER_H_
#pragma once

#include "content/public/browser/browser_message_filter.h"
#include "ppapi/c/pp_resource.h"

// A message filter for Gtalk-specific pepper messages.
class PepperGtalkMessageFilter : public content::BrowserMessageFilter {
 public:
  PepperGtalkMessageFilter();

  // content::BrowserMessageFilter methods.
  virtual void OverrideThreadForMessage(
      const IPC::Message& message,
      content::BrowserThread::ID* thread) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

 private:
  virtual ~PepperGtalkMessageFilter();

  void OnTalkGetPermission(uint32 plugin_dispatcher_id,
                           PP_Resource resource);

  DISALLOW_COPY_AND_ASSIGN(PepperGtalkMessageFilter);
};

#endif  // CHROME_BROWSER_PEPPER_GTALK_MESSAGE_FILTER_H_
