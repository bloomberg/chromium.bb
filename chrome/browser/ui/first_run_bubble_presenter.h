// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FIRST_RUN_BUBBLE_PRESENTER_H_
#define CHROME_BROWSER_UI_FIRST_RUN_BUBBLE_PRESENTER_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/sessions/core/session_id.h"

class Browser;
class TemplateURLService;

// This class is responsible for showing the first run bubble once the template
// URL service is ready, since the template URL service is needed to fill in the
// bubble.
class FirstRunBubblePresenter : public TemplateURLServiceObserver {
 public:
  static void PresentWhenReady(Browser* browser);

 private:
  FirstRunBubblePresenter(TemplateURLService* url_service, Browser* browser);

  ~FirstRunBubblePresenter() override;

  // TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;
  void OnTemplateURLServiceShuttingDown() override;

  TemplateURLService* template_url_service_;
  SessionID session_id_;
  ScopedObserver<TemplateURLService, TemplateURLServiceObserver>
      scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunBubblePresenter);
};

#endif  // CHROME_BROWSER_UI_FIRST_RUN_BUBBLE_PRESENTER_H_
