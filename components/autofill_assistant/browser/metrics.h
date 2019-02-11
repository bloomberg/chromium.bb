// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_METRICS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_METRICS_H_

#include <ostream>

namespace autofill_assistant {

// A class to generate Autofill Assistant related histograms.
class Metrics {
 public:
  // The different ways that autofill assistant can stop.
  //
  // GENERATED_JAVA_ENUM_PACKAGE: (
  // org.chromium.chrome.browser.autofill_assistant.metrics)
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: DropOutReason
  enum DropOutReason {
    AA_START = 0,
    AUTOSTART_TIMEOUT = 1,
    NO_SCRIPTS = 2,
    CUSTOM_TAB_CLOSED = 3,
    DECLINED = 4,
    SHEET_CLOSED = 5,
    SCRIPT_FAILED = 6,
    NAVIGATION = 7,
    OVERLAY_STOP = 8,
    PR_FAILED = 9,
    CONTENT_DESTROYED = 10,
    RENDER_PROCESS_GONE = 11,
    INTERSTITIAL_PAGE = 12,
    SCRIPT_SHUTDOWN = 13,
    SAFETY_NET_TERMINATE = 14,  // This is a "should never happen" entry.
    TAB_DETACHED = 15,
    TAB_CHANGED = 16,
    NUM_ENTRIES = 17,
  };

  static void RecordDropOut(DropOutReason reason);

  // Intended for debugging: writes string representation of |reason| to |out|.
  friend std::ostream& operator<<(std::ostream& out,
                                  const DropOutReason& reason) {
#ifdef NDEBUG
    // Non-debugging builds write the enum number.
    out << static_cast<int>(reason);
    return out;
#else
    // Debugging builds write a string representation of |reason|.
    switch (reason) {
      case AA_START:
        out << "AA_START";
        break;
      case AUTOSTART_TIMEOUT:
        out << "AUTOSTART_TIMEOUT";
        break;
      case NO_SCRIPTS:
        out << "NO_SCRIPTS";
        break;
      case CUSTOM_TAB_CLOSED:
        out << "CUSTOM_TAB_CLOSED";
        break;
      case DECLINED:
        out << "DECLINED";
        break;
      case SHEET_CLOSED:
        out << "SHEET_CLOSED";
        break;
      case SCRIPT_FAILED:
        out << "SCRIPT_FAILED";
        break;
      case NAVIGATION:
        out << "NAVIGATION";
        break;
      case OVERLAY_STOP:
        out << "OVERLAY_STOP";
        break;
      case PR_FAILED:
        out << "PR_FAILED";
        break;
      case CONTENT_DESTROYED:
        out << "CONTENT_DESTROYED";
        break;
      case RENDER_PROCESS_GONE:
        out << "RENDER_PROCESS_GONE";
        break;
      case INTERSTITIAL_PAGE:
        out << "INTERSTITIAL_PAGE";
        break;
      case SCRIPT_SHUTDOWN:
        out << "SCRIPT_SHUTDOWN";
        break;
      case SAFETY_NET_TERMINATE:
        out << "SAFETY_NET_TERMINATE";
        break;
      case TAB_DETACHED:
        out << "TAB_DETACHED";
        break;
      case TAB_CHANGED:
        out << "TAB_CHANGED";
        break;
      case NUM_ENTRIES:
        out << "NUM_ENTRIES";
        // Intentionally no default case to make compilation fail if a new value
        // was added to the enum but not to this list.
    }
    return out;
#endif  // NDEBUG
  }
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_METRICS_H_
