// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_service.h"

#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"

InstantService::InstantService() {
  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllSources());
}

InstantService::~InstantService() {
}

void InstantService::AddInstantProcess(int process_id) {
  process_ids_.insert(process_id);
}

bool InstantService::IsInstantProcess(int process_id) const {
  return process_ids_.find(process_id) != process_ids_.end();
}

void InstantService::Shutdown() {
  process_ids_.clear();
}

void InstantService::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  int process_id = content::Source<content::RenderProcessHost>(source)->GetID();
  process_ids_.erase(process_id);
}
