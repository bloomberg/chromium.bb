// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_GZIPPED_PROTOBUFS_FIELD_TRIAL_H_
#define CHROME_BROWSER_METRICS_GZIPPED_PROTOBUFS_FIELD_TRIAL_H_

namespace metrics {

// Create a field trial for selecting whether to gzip protobufs before uploading
// or not.
void CreateGzippedProtobufsFieldTrial();

// Returns whether we are in a field trial group that compresses protobufs
// before uploading.
bool ShouldGzipProtobufsBeforeUploading();

}  // namespace metrics


#endif  // CHROME_BROWSER_METRICS_GZIPPED_PROTOBUFS_FIELD_TRIAL_H_
