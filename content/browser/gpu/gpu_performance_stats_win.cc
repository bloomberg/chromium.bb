// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_performance_stats.h"

#include <winsatcominterfacei.h>

static float GetComponentScore(IProvideWinSATAssessmentInfo* subcomponent) {
  float score;
  HRESULT hr = subcomponent->get_Score(&score);
  if (FAILED(hr))
    return 0.0;
  return score;
}

GpuPerformanceStats GpuPerformanceStats::RetrieveGpuPerformanceStats() {
  HRESULT hr = S_OK;
  IQueryRecentWinSATAssessment* assessment = NULL;
  IProvideWinSATResultsInfo* results = NULL;
  IProvideWinSATAssessmentInfo* subcomponent = NULL;
  GpuPerformanceStats stats;

  hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  if (FAILED(hr))
    goto cleanup;

  hr = CoCreateInstance(__uuidof(CQueryWinSAT),
                        NULL,
                        CLSCTX_INPROC_SERVER,
                        __uuidof(IQueryRecentWinSATAssessment),
                        reinterpret_cast<void**>(&assessment));
  if (FAILED(hr))
    goto cleanup;

  hr = assessment->get_Info(&results);
  if (FAILED(hr))
    goto cleanup;

  hr = results->get_SystemRating(&stats.overall);
  if (FAILED(hr))
    goto cleanup;

  hr = results->GetAssessmentInfo(WINSAT_ASSESSMENT_D3D, &subcomponent);
  if (FAILED(hr))
    goto cleanup;
  stats.gaming = GetComponentScore(subcomponent);
  subcomponent->Release();
  subcomponent = NULL;

  hr = results->GetAssessmentInfo(WINSAT_ASSESSMENT_GRAPHICS, &subcomponent);
  if (FAILED(hr))
    goto cleanup;
  stats.graphics = GetComponentScore(subcomponent);
  subcomponent->Release();
  subcomponent = NULL;

 cleanup:
  if (assessment)
    assessment->Release();
  if (results)
    results->Release();
  if (subcomponent)
    subcomponent->Release();
  CoUninitialize();

  return stats;
}
