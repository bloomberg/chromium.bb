// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_ACCELERATORS_GTK_H_
#define CHROME_BROWSER_UI_GTK_ACCELERATORS_GTK_H_

#include "base/hash_tables.h"
#include "ui/base/accelerators/accelerator.h"

template <typename T> struct DefaultSingletonTraits;

class AcceleratorsGtk {
 public:
  typedef std::vector<std::pair<int, ui::Accelerator> > AcceleratorList;
  typedef AcceleratorList::const_iterator const_iterator;

  // Returns the singleton instance.
  static AcceleratorsGtk* GetInstance();

  const_iterator const begin() { return all_accelerators_.begin(); }
  const_iterator const end() { return all_accelerators_.end(); }

  // Returns NULL if there is no accelerator for the command.
  const ui::Accelerator* GetPrimaryAcceleratorForCommand(int command_id);

 private:
  friend struct DefaultSingletonTraits<AcceleratorsGtk>;

  AcceleratorsGtk();
  ~AcceleratorsGtk();

  typedef base::hash_map<int, ui::Accelerator> AcceleratorMap;
  AcceleratorMap primary_accelerators_;

  AcceleratorList all_accelerators_;
};

#endif  // CHROME_BROWSER_UI_GTK_ACCELERATORS_GTK_H_
