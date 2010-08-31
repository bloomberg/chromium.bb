// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_MODEL_OBSERVER_H_
#define CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_MODEL_OBSERVER_H_
#pragma once

// TemplateURLModelObserver is notified whenever the set of TemplateURLs
// are modified.
class TemplateURLModelObserver {
 public:
  // Notification that the template url model has changed in some way.
  virtual void OnTemplateURLModelChanged() = 0;

 protected:
  virtual ~TemplateURLModelObserver() {}
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_MODEL_OBSERVER_H_
