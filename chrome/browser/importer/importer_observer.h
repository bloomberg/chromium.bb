// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IMPORTER_OBSERVER_H_
#define CHROME_BROWSER_IMPORTER_IMPORTER_OBSERVER_H_
#pragma once

// An interface that notifies the consumers that the import process was
// completed.
class ImporterObserver {
 public:
  virtual ~ImporterObserver() {}

  // The import operation was completed successfully.
  virtual void ImportCompleted() = 0;
};

#endif  // CHROME_BROWSER_IMPORTER_IMPORTER_OBSERVER_H_
