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


#include "base/metrics/histogram.h"
#include "base/metrics/nacl_histogram.h"

// To log histogram data about NaCl.Startups, call this function with
// a NaClHistogramValue passed in as |hvalue|
void UmaNaclHistogramEnumeration(NaClHistogramValue histogram_value) {
  UMA_HISTOGRAM_ENUMERATION("NaCl.Startups", histogram_value,
    NACL_MAX_HISTOGRAM);
}

