// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

// Include once to get the type definitions
#include "chrome/common/all_messages.h"

struct msginfo {
  const char* name;
  int id;
  int in_count;
  int out_count;

  bool operator< (const msginfo other) const {
    return id < other.id;
  }
};

// Redefine macros to generate table
#include "ipc/ipc_message_null_macros.h"
#undef IPC_MESSAGE_DECL
#define IPC_MESSAGE_DECL(kind, type, name, in, out, ilist, olist) \
  { #name, IPC_MESSAGE_ID(), in, out },

static msginfo msgtable[] = {
#include "chrome/common/all_messages.h"
};
#define MSGTABLE_SIZE (sizeof(msgtable)/sizeof(msgtable[0]))

static bool check_msgtable() {
  bool result = true;
  int previous_class_id = 0;
  int highest_class_id = 0;
  std::vector<int> exemptions;

  // Exclude test files from consideration.  Do not include message
  // files used inside the actual chrome browser in this list.
  exemptions.push_back(TestMsgStart);
  exemptions.push_back(FirefoxImporterUnittestMsgStart);

  for (size_t i = 0; i < MSGTABLE_SIZE; ++i) {
    int class_id = IPC_MESSAGE_ID_CLASS(msgtable[i].id);
    if (class_id >= LastIPCMsgStart) {
      std::cout << "Invalid LastIPCMsgStart setting\n";
      result = false;
    }
    while (class_id > previous_class_id + 1) {
      std::vector<int>::iterator iter;
      iter = find(exemptions.begin(), exemptions.end(), previous_class_id+1);
      if (iter == exemptions.end()) {
        std::cout << "Missing message file: gap before " << class_id << "\n";
        result = false;
        break;
      }
      ++previous_class_id;
    }
    previous_class_id = class_id;
    if (class_id > highest_class_id)
      highest_class_id = class_id;
  }

  if (LastIPCMsgStart > highest_class_id + 1) {
      std::cout << "Missing message file: gap before LastIPCMsgStart\n";
      result = false;
  }

  if (!result)
    std::cout << "Please check chrome/common/all_messages.h.\n";

  return result;
}

static void dump_msgtable(bool show_args, bool show_ids,
                          bool show_comma, const char *prefix) {
  bool first = true;
  for (size_t i = 0; i < MSGTABLE_SIZE; ++i) {
    if ((!prefix) || strstr(msgtable[i].name, prefix) == msgtable[i].name) {
      if (show_comma) {
        if (!first)
          std::cout << ",";
        first = false;
        std::cout << msgtable[i].id;
      } else {
        if (show_ids)
          std::cout << msgtable[i].id << " " <<
              IPC_MESSAGE_ID_CLASS(msgtable[i].id) << "," <<
              IPC_MESSAGE_ID_LINE(msgtable[i].id) << " ";
        std::cout << msgtable[i].name;
        if (show_args) {
          std::cout << "(" << msgtable[i].in_count << " IN, "  <<
              msgtable[i].out_count << " OUT)";
        }
        std::cout << "\n";
      }
    }
  }
  if (show_comma)
    std::cout << "\n";
}

int main(int argc, char **argv) {
  bool show_args = false;
  bool show_ids  = false;
  bool skip_check = false;
  bool show_comma = false;
  const char *filter = NULL;

  while (--argc > 0) {
    ++argv;
    if (std::string("--args") == *argv) {
      show_args = true;
    } else if (std::string("--comma") == *argv) {
      show_comma = true;
    } else if (std::string("--filter") == *argv) {
      filter = *(++argv);
      --argc;
    } else if (std::string("--ids") == *argv) {
      show_ids = true;
    } else if (std::string("--no-check") == *argv) {
      skip_check = true;
    } else {
      std::cout <<
          "usage: ipclist [--args] [--ids] [--no-check] [--filter prefix] "
          "[--comma]\n";
      return 1;
    }
  }

  std::sort(msgtable, msgtable + MSGTABLE_SIZE);

  if (!skip_check && check_msgtable() == false)
    return 1;

  dump_msgtable(show_args, show_ids, show_comma, filter);
  return 0;
}

