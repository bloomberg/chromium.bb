// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/metrics/uma_session_stats.h"

#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/prefs/pref_service.h"
#include "components/variations/metrics_util.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/user_metrics.h"
#include "jni/UmaSessionStats_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::UserMetricsAction;

namespace {
UmaSessionStats* g_uma_session_stats = NULL;
}  // namespace

UmaSessionStats::UmaSessionStats()
    : active_session_count_(0) {
}

UmaSessionStats::~UmaSessionStats() {
}

void UmaSessionStats::UmaResumeSession(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  DCHECK(g_browser_process);

  if (active_session_count_ == 0) {
    session_start_time_ = base::TimeTicks::Now();

    // Tell the metrics service that the application resumes.
    metrics::MetricsService* metrics = g_browser_process->metrics_service();
    if (metrics) {
      metrics->OnAppEnterForeground();
    }
  }
  ++active_session_count_;
}

void UmaSessionStats::UmaEndSession(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj) {
  --active_session_count_;
  DCHECK_GE(active_session_count_, 0);

  if (active_session_count_ == 0) {
    base::TimeDelta duration = base::TimeTicks::Now() - session_start_time_;
    UMA_HISTOGRAM_LONG_TIMES("Session.TotalDuration", duration);

    DCHECK(g_browser_process);
    // Tell the metrics service it was cleanly shutdown.
    metrics::MetricsService* metrics = g_browser_process->metrics_service();
    if (metrics) {
      metrics->OnAppEnterBackground();
    }
  }
}

// static
void UmaSessionStats::RegisterSyntheticFieldTrialWithNameHash(
    uint32_t trial_name_hash,
    const std::string& group_name) {
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrialWithNameHash(
      trial_name_hash, group_name);
}

// static
void UmaSessionStats::RegisterSyntheticFieldTrial(
    const std::string& trial_name,
    const std::string& group_name) {
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(trial_name,
                                                            group_name);
}

// Starts/stops the MetricsService when permissions have changed.
// There are three possible states:
// * Logs are being recorded and being uploaded to the server.
// * Logs are being recorded, but not being uploaded to the server.
//   This happens when we've got permission to upload on Wi-Fi but we're on a
//   mobile connection (for example).
// * Logs are neither being recorded or uploaded.
static void UpdateMetricsServiceState(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj,
                                      jboolean may_record,
                                      jboolean may_upload) {
  metrics::MetricsService* metrics = g_browser_process->metrics_service();
  DCHECK(metrics);

  if (metrics->recording_active() != may_record) {
    // This function puts a consent file with the ClientID in the
    // data directory. The ID is passed to the renderer for crash
    // reporting when things go wrong.
    content::BrowserThread::GetBlockingPool()->PostTask(FROM_HERE,
        base::Bind(
            base::IgnoreResult(GoogleUpdateSettings::SetCollectStatsConsent),
            may_record));
  }

  g_browser_process->GetMetricsServicesManager()->UpdatePermissions(
      may_record, may_upload);
}

// Renderer process crashed in the foreground.
static void LogRendererCrash(JNIEnv*, const JavaParamRef<jclass>&) {
  DCHECK(g_browser_process);
  // Increment the renderer crash count in stability metrics.
  PrefService* pref = g_browser_process->local_state();
  DCHECK(pref);
  int value = pref->GetInteger(metrics::prefs::kStabilityRendererCrashCount);
  pref->SetInteger(metrics::prefs::kStabilityRendererCrashCount, value + 1);
}

static void RegisterExternalExperiment(JNIEnv* env,
                                       const JavaParamRef<jclass>& clazz,
                                       jint study_id,
                                       jint experiment_id) {
  const std::string group_name_utf8 = base::IntToString(experiment_id);

  variations::ActiveGroupId active_group;
  active_group.name = static_cast<uint32_t>(study_id);
  active_group.group = metrics::HashName(group_name_utf8);
  variations::AssociateGoogleVariationIDForceHashes(
      variations::GOOGLE_WEB_PROPERTIES, active_group,
      static_cast<variations::VariationID>(experiment_id));

  UmaSessionStats::RegisterSyntheticFieldTrialWithNameHash(
      static_cast<uint32_t>(study_id), group_name_utf8);
}

static void RegisterSyntheticFieldTrial(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& jtrial_name,
    const JavaParamRef<jstring>& jgroup_name) {
  std::string trial_name(ConvertJavaStringToUTF8(env, jtrial_name));
  std::string group_name(ConvertJavaStringToUTF8(env, jgroup_name));
  UmaSessionStats::RegisterSyntheticFieldTrial(trial_name, group_name);
}

static void RecordMultiWindowSession(JNIEnv*,
                                     const JavaParamRef<jclass>&,
                                     jint area_percent,
                                     jint instance_count) {
  UMA_HISTOGRAM_PERCENTAGE("MobileStartup.MobileMultiWindowSession",
                           area_percent);
  // Make sure the bucket count is the same as the range.  This currently
  // expects no more than 10 simultaneous multi window instances.
  UMA_HISTOGRAM_CUSTOM_COUNTS("MobileStartup.MobileMultiWindowInstances",
                              instance_count,
                              1 /* min */,
                              10 /* max */,
                              10 /* bucket count */);
}

static void RecordTabCountPerLoad(JNIEnv*,
                                  const JavaParamRef<jclass>&,
                                  jint num_tabs) {
  // Record how many tabs total are open.
  UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.TabCountPerLoad", num_tabs, 1, 200, 50);
}

static void RecordPageLoaded(JNIEnv*,
                             const JavaParamRef<jclass>&,
                             jboolean is_desktop_user_agent) {
  // Should be called whenever a page has been loaded.
  content::RecordAction(UserMetricsAction("MobilePageLoaded"));
  if (is_desktop_user_agent) {
    content::RecordAction(
        UserMetricsAction("MobilePageLoadedDesktopUserAgent"));
  }
}

static void RecordPageLoadedWithKeyboard(JNIEnv*, const JavaParamRef<jclass>&) {
  content::RecordAction(UserMetricsAction("MobilePageLoadedWithKeyboard"));
}

static jlong Init(JNIEnv* env, const JavaParamRef<jclass>& obj) {
  // We should have only one UmaSessionStats instance.
  DCHECK(!g_uma_session_stats);
  g_uma_session_stats = new UmaSessionStats();
  return reinterpret_cast<intptr_t>(g_uma_session_stats);
}

// Register native methods
bool RegisterUmaSessionStats(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
