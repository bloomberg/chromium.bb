// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_MAIN_SAMPLE_ACTIVITY_H_
#define ATHENA_MAIN_SAMPLE_ACTIVITY_H_

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_view_model.h"
#include "base/memory/scoped_ptr.h"

class SampleActivity : public athena::Activity,
                       public athena::ActivityViewModel {
 public:
  SampleActivity(SkColor color, SkColor content, const std::string& title);
  virtual ~SampleActivity();

 private:
  // athena::Activity:
  virtual athena::ActivityViewModel* GetActivityViewModel() OVERRIDE;

  // athena::ActivityViewModel:
  virtual SkColor GetRepresentativeColor() OVERRIDE;
  virtual std::string GetTitle() OVERRIDE;
  virtual aura::Window* GetNativeWindow() OVERRIDE;

  SkColor color_;
  SkColor content_color_;
  std::string title_;
  scoped_ptr<aura::Window> window_;

  DISALLOW_COPY_AND_ASSIGN(SampleActivity);
};

#endif  // ATHENA_MAIN_SAMPLE_ACTIVITY_H_
