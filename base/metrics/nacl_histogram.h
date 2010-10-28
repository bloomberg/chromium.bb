// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// nacl_histogram provides an enum and defines a macro that uses definitions
// from histogram.h. Note that a histogram is an object that aggregates
// statistics, and can summarize them in various forms, including ASCII
// graphical, HTML, and numerically.
// nacl_histogram information is used to compare how many times a native
// client module has been loaded, compared to the number of chrome starts
// and number of new tabs created.

#ifndef BASE_METRICS_NACL_HISTOGRAM_H_
#define BASE_METRICS_NACL_HISTOGRAM_H_
#pragma once

enum NaClHistogramValue {
  FIRST_TAB_NACL_BASELINE,   // First tab created - a baseline for NaCl starts.
  NEW_TAB_NACL_BASELINE,     // New tab created -- a baseline for NaCl starts.
  NACL_STARTED,              // NaCl process started
  NACL_MAX_HISTOGRAM         // DO NOT USE -- used by histogram for max bound.
};

// To log histogram data about NaCl.Startups, call this macro with
// a NaClHistogramValue passed in as |histogram_value|
void UmaNaclHistogramEnumeration(NaClHistogramValue histogram_value);

#endif  // BASE_METRICS_NACL_HISTOGRAM_H_

