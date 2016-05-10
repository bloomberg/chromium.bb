// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_RENDER_THREAD_OBSERVER_H_
#define CHROME_RENDERER_CHROME_RENDER_THREAD_OBSERVER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/process/process.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/renderer/render_thread_observer.h"

class GURL;
struct ContentSettings;

namespace base {
class CommandLine;
}

namespace content {
class ResourceDispatcherDelegate;
}

// This class filters the incoming control messages (i.e. ones not destined for
// a RenderView) for Chrome specific messages that the content layer doesn't
// happen.  If a few messages are related, they should probably have their own
// observer.
class ChromeRenderThreadObserver : public content::RenderThreadObserver,
                                    public base::FieldTrialList::Observer {
 public:
  ChromeRenderThreadObserver();
  ~ChromeRenderThreadObserver() override;

  static bool is_incognito_process() { return is_incognito_process_; }

  // Returns a pointer to the content setting rules owned by
  // |ChromeRenderThreadObserver|.
  const RendererContentSettingRules* content_setting_rules() const;

 private:
  // Initializes field trial state change observation and notifies the browser
  // of any field trials that might have already been activated.
  void InitFieldTrialObserving(const base::CommandLine& command_line);

  // content::RenderThreadObserver:
  bool OnControlMessageReceived(const IPC::Message& message) override;

  // base::FieldTrialList::Observer:
  void OnFieldTrialGroupFinalized(const std::string& trial_name,
                                  const std::string& group_name) override;

  void OnSetIsIncognitoProcess(bool is_incognito_process);
  void OnSetContentSettingsForCurrentURL(
      const GURL& url, const ContentSettings& content_settings);
  void OnSetContentSettingRules(const RendererContentSettingRules& rules);
  void OnGetCacheResourceStats();
  void OnSetFieldTrialGroup(const std::string& fiel_trial_name,
                            const std::string& group_name,
                            base::ProcessId sender_pid,
                            int32_t debug_token);

  static bool is_incognito_process_;
  std::unique_ptr<content::ResourceDispatcherDelegate> resource_delegate_;
  RendererContentSettingRules content_setting_rules_;

  base::WeakPtrFactory<ChromeRenderThreadObserver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeRenderThreadObserver);
};

#endif  // CHROME_RENDERER_CHROME_RENDER_THREAD_OBSERVER_H_
