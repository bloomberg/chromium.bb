// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_CONFIG_IMPL_H_
#define CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_CONFIG_IMPL_H_

#include "base/memory/scoped_vector.h"
#include "content/common/content_export.h"
#include "content/public/browser/background_tracing_config.h"

namespace content {
class BackgroundTracingRule;

class CONTENT_EXPORT BackgroundTracingConfigImpl
    : public BackgroundTracingConfig {
 public:
  explicit BackgroundTracingConfigImpl(TracingMode tracing_mode);

  ~BackgroundTracingConfigImpl() override;

  // From BackgroundTracingConfig
  void IntoDict(base::DictionaryValue* dict) const override;

  enum CategoryPreset {
    BENCHMARK,
    BENCHMARK_DEEP,
    BENCHMARK_GPU,
    BENCHMARK_IPC,
    BENCHMARK_STARTUP,
  };

  CategoryPreset category_preset() const { return category_preset_; }
  void set_category_preset(CategoryPreset category_preset) {
    category_preset_ = category_preset;
  }

  const ScopedVector<BackgroundTracingRule>& rules() const { return rules_; }

  void AddPreemptiveRule(const base::DictionaryValue* dict);
  void AddReactiveRule(
      const base::DictionaryValue* dict,
      BackgroundTracingConfigImpl::CategoryPreset category_preset);

  static scoped_ptr<BackgroundTracingConfigImpl> PreemptiveFromDict(
      const base::DictionaryValue* dict);
  static scoped_ptr<BackgroundTracingConfigImpl> ReactiveFromDict(
      const base::DictionaryValue* dict);

  static scoped_ptr<BackgroundTracingConfigImpl> FromDict(
      const base::DictionaryValue* dict);

  static std::string CategoryPresetToString(
      BackgroundTracingConfigImpl::CategoryPreset category_preset);
  static bool StringToCategoryPreset(
      const std::string& category_preset_string,
      BackgroundTracingConfigImpl::CategoryPreset* category_preset);

 private:
  CategoryPreset category_preset_;
  ScopedVector<BackgroundTracingRule> rules_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundTracingConfigImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_CONFIG_IMPL_H_
