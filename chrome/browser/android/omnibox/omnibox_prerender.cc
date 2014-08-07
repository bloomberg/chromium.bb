// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "omnibox_prerender.h"

#include "base/android/jni_string.h"
#include "base/logging.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/autocomplete/autocomplete_result.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/ui/search/instant_search_prerenderer.h"
#include "components/omnibox/autocomplete_match.h"
#include "content/public/browser/web_contents.h"
#include "jni/OmniboxPrerender_jni.h"
#include "url/gurl.h"

using predictors::AutocompleteActionPredictor;
using predictors::AutocompleteActionPredictorFactory;

OmniboxPrerender::OmniboxPrerender(JNIEnv* env, jobject obj)
    : weak_java_omnibox_(env, obj) {
}

OmniboxPrerender::~OmniboxPrerender() {
}

static jlong Init(JNIEnv* env, jobject obj) {
  OmniboxPrerender* omnibox = new OmniboxPrerender(env, obj);
  return reinterpret_cast<intptr_t>(omnibox);
}

bool RegisterOmniboxPrerender(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void OmniboxPrerender::Clear(JNIEnv* env,
                             jobject obj,
                             jobject j_profile_android) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile_android);
  DCHECK(profile);
  if (!profile)
    return;
  AutocompleteActionPredictor* action_predictor =
      AutocompleteActionPredictorFactory::GetForProfile(profile);
  action_predictor->ClearTransitionalMatches();
  action_predictor->CancelPrerender();
}

void OmniboxPrerender::InitializeForProfile(
    JNIEnv* env,
    jobject obj,
    jobject j_profile_android) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile_android);
  // Initialize the AutocompleteActionPredictor for this profile.
  // It needs to register for notifications as part of its initialization.
  AutocompleteActionPredictorFactory::GetForProfile(profile);
}

void OmniboxPrerender::PrerenderMaybe(JNIEnv* env,
                                      jobject obj,
                                      jstring j_url,
                                      jstring j_current_url,
                                      jlong jsource_match,
                                      jobject j_profile_android,
                                      jobject j_tab) {
  AutocompleteResult* autocomplete_result =
      reinterpret_cast<AutocompleteResult*>(jsource_match);
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile_android);
  base::string16 url_string =
      base::android::ConvertJavaStringToUTF16(env, j_url);
  base::string16 current_url_string =
      base::android::ConvertJavaStringToUTF16(env, j_current_url);
  content::WebContents* web_contents =
      TabAndroid::GetNativeTab(env, j_tab)->web_contents();
  // TODO(apiccion) Use a delegate for communicating with web_contents.
  // This can happen in OmniboxTests since the results are generated
  // in Java only.
  if (!autocomplete_result)
    return;
  if (!profile)
    return;

  const AutocompleteResult::const_iterator default_match(
      autocomplete_result->default_match());
  if (default_match == autocomplete_result->end())
    return;

  AutocompleteActionPredictor::Action recommended_action =
      AutocompleteActionPredictor::ACTION_NONE;
  InstantSearchPrerenderer* prerenderer =
      InstantSearchPrerenderer::GetForProfile(profile);
  if (prerenderer &&
      prerenderer->IsAllowed(*default_match, web_contents)) {
    recommended_action = AutocompleteActionPredictor::ACTION_PRERENDER;
  } else {
    AutocompleteActionPredictor* action_predictor =
        AutocompleteActionPredictorFactory::GetForProfile(profile);
    if (!action_predictor)
      return;

    if (action_predictor) {
      action_predictor->
          RegisterTransitionalMatches(url_string, *autocomplete_result);
      recommended_action =
          action_predictor->RecommendAction(url_string, *default_match);
    }
  }

  GURL current_url = GURL(current_url_string);
  switch (recommended_action) {
    case AutocompleteActionPredictor::ACTION_PRERENDER:
      // Ask for prerendering if the destination URL is different than the
      // current URL.
      if (default_match->destination_url != current_url) {
        DoPrerender(
            *default_match,
            profile,
            web_contents);
      }
      break;
    case AutocompleteActionPredictor::ACTION_PRECONNECT:
      // TODO (apiccion) add preconnect logic
      break;
    case AutocompleteActionPredictor::ACTION_NONE:
      break;
    default:
      NOTREACHED();
      break;
  }
}

void OmniboxPrerender::DoPrerender(const AutocompleteMatch& match,
                                   Profile* profile,
                                   content::WebContents* web_contents) {
  DCHECK(profile);
  if (!profile)
    return;
  DCHECK(web_contents);
  if (!web_contents)
    return;
  gfx::Rect container_bounds = web_contents->GetContainerBounds();
  InstantSearchPrerenderer* prerenderer =
      InstantSearchPrerenderer::GetForProfile(profile);
  if (prerenderer && prerenderer->IsAllowed(match, web_contents)) {
    prerenderer->Init(
        web_contents->GetController().GetSessionStorageNamespaceMap(),
        container_bounds.size());
    return;
  }
  predictors::AutocompleteActionPredictorFactory::GetForProfile(profile)->
      StartPrerendering(
          match.destination_url,
          web_contents->GetController().GetSessionStorageNamespaceMap(),
          container_bounds.size());
}
