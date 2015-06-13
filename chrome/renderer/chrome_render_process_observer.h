// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_RENDER_PROCESS_OBSERVER_H_
#define CHROME_RENDERER_CHROME_RENDER_PROCESS_OBSERVER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/renderer/render_process_observer.h"

class GURL;
struct ContentSettings;

namespace content {
class ResourceDispatcherDelegate;
}

// This class filters the incoming control messages (i.e. ones not destined for
// a RenderView) for Chrome specific messages that the content layer doesn't
// happen.  If a few messages are related, they should probably have their own
// observer.
class ChromeRenderProcessObserver : public content::RenderProcessObserver,
                                    public base::FieldTrialList::Observer {
 public:
  ChromeRenderProcessObserver();
  ~ChromeRenderProcessObserver() override;

  static bool is_incognito_process() { return is_incognito_process_; }

  bool webkit_initialized() const { return webkit_initialized_; }

  // Returns a pointer to the content setting rules owned by
  // |ChromeRenderProcessObserver|.
  const RendererContentSettingRules* content_setting_rules() const;

 private:
  // RenderProcessObserver implementation.
  bool OnControlMessageReceived(const IPC::Message& message) override;
  void WebKitInitialized() override;
  void OnRenderProcessShutdown() override;

  // Observer implementation.
  void OnFieldTrialGroupFinalized(const std::string& trial_name,
                                  const std::string& group_name) override;

  void OnSetIsIncognitoProcess(bool is_incognito_process);
  void OnSetContentSettingsForCurrentURL(
      const GURL& url, const ContentSettings& content_settings);
  void OnSetContentSettingRules(const RendererContentSettingRules& rules);
  void OnGetCacheResourceStats();
  void OnSetFieldTrialGroup(const std::string& fiel_trial_name,
                            const std::string& group_name);

  static bool is_incognito_process_;
  scoped_ptr<content::ResourceDispatcherDelegate> resource_delegate_;
  RendererContentSettingRules content_setting_rules_;

  bool webkit_initialized_;

  base::WeakPtrFactory<ChromeRenderProcessObserver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeRenderProcessObserver);
};

#endif  // CHROME_RENDERER_CHROME_RENDER_PROCESS_OBSERVER_H_
