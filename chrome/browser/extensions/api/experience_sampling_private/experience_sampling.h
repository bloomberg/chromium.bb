// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EXPERIENCE_SAMPLING_PRIVATE_EXPERIENCE_SAMPLING_H_
#define CHROME_BROWSER_EXTENSIONS_API_EXPERIENCE_SAMPLING_PRIVATE_EXPERIENCE_SAMPLING_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/common/extensions/api/experience_sampling_private.h"

namespace content {
class BrowserContext;
}

class GURL;

namespace extensions {

using api::experience_sampling_private::UIElement;

class ExperienceSamplingEvent {
 public:
  static const char kProceed[];
  static const char kDeny[];
  static const char kCancel[];
  static const char kReload[];

  static scoped_ptr<ExperienceSamplingEvent> Create(
      const std::string& element_name,
      const GURL& destination,
      const GURL& referrer);

  static scoped_ptr<ExperienceSamplingEvent> Create(
      const std::string& element_name);

  ExperienceSamplingEvent(const std::string& element_name,
                          const GURL& destination,
                          const GURL& referrer,
                          content::BrowserContext* browser_context);
  ~ExperienceSamplingEvent();

  void CreateOnDisplayedEvent();
  void CreateUserDecisionEvent(const std::string& decision_name);

  bool has_viewed_details() const { return has_viewed_details_; }
  void set_has_viewed_details(bool viewed) { has_viewed_details_ = viewed; }
  bool has_viewed_learn_more() const { return has_viewed_learn_more_; }
  void set_has_viewed_learn_more(bool viewed) {
    has_viewed_learn_more_ = viewed;
  }

 private:
  bool has_viewed_details_;
  bool has_viewed_learn_more_;
  content::BrowserContext* browser_context_;
  UIElement ui_element_;

  DISALLOW_COPY_AND_ASSIGN(ExperienceSamplingEvent);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXPERIENCE_SAMPLING_PRIVATE_EXPERIENCE_SAMPLING_H_
