# Chrome Network Bug Triage : Suggested Workflow

[TOC]

## Looking for new crashers

1. Go to [go/chromecrash](https://goto.google.com/chromecrash).

2. For each platform, look through the releases for which releases to
   investigate.  As per bug-triage.txt, this should be the most recent canary,
   the previous canary (if the most recent is less than a day old), and any of
   dev/beta/stable that were released in the last couple of days.

3. For each release, in the "Process Type" frame, click on "browser".

4. At the bottom of the "Magic Signature" frame,  click "limit 1000".  Reported
   crashers are sorted in decreasing order of the number of reports for that
   crash signature.

5. Search the page for *"net::"*.

6. For each found signature:
    * If there is a bug already filed, make sure it is correctly describing the
      current bug (e.g. not closed, or not describing a long-past issue), and
      make sure that if it is a *net* bug, that it is labeled as such.
    * Ignore signatures that only occur once, as memory corruption can easily
      cause one-off failures when the sample size is large enough.
    * Ignore signatures that only come from a single client ID, as individual
      machine malware and breakage can also easily cause one-off failures.
    * Click on the number of reports field to see details of crash. Ignore it
      if it doesn't appear to be a network bug.
    * Otherwise, file a new bug directly from chromecrash.  Note that this may
      result in filing bugs for low- and very-low- frequency crashes.  That's
      ok; the bug tracker is a better tool to figure out whether or not we put
      resources into those crashes than a snap judgement when filing bugs.
    * For each bug you file, include the following information:
        * The backtrace.  Note that the backtrace should not be added to the
          bug if Restrict-View-Google isn't set on the bug as it may contain
          PII.  Filing the bug from the crash reporter should do this
          automatically, but check.
        * The channel in which the bug is seen (canary/dev/beta/stable), its
          frequency in that channel, and its rank among crashers in the
          channel.
        * The frequency of this signature in recent releases.  This information
          is available by:
            1. Clicking on the signature in the "Magic Signature" list
            2. Clicking "Edit" on the dremel query at the top of the page
            3. Removing the "product.version='X.Y.Z.W' AND" string and clicking
               "Update".
            4. Clicking "Limit 1000" in the Product Version list in the
               resulting page (without this, the listing will be restricted to
               the releases in which the signature is most common, which will
               often not include the canary/dev release being investigated).
            5. Choose some subset of that list, or all of it, to include in the
               bug.  Make sure to indicate if there is a defined point in the
               past before which the signature is not present.

## Identifying unlabeled network bugs on the tracker

* Look at new uncomfirmed bugs since noon PST on the last triager's rotation.
  [Use this issue tracker
  query](https://code.google.com/p/chromium/issues/list?can=2&q=status%3Aunconfirmed&sort=-id&num=1000).

* Press **h** to bring up a preview of the bug text.

* Use **j** and **k** to advance through bugs.

* If a bug looks like it might be network/download/safe-browsing related,
  middle click (or command-click on OSX) to open in new tab.

* If a user provides a crash ID for a crasher for a bug that could be
  net-related, look at the crash stack at
  [go/crash](https://goto.google.com/crash), and see if it looks to be network
  related.  Be sure to check if other bug reports have that stack trace, and
  mark as a dupe if so.  Even if the bug isn't network related, paste the stack
  trace in the bug, so no one else has to look up the crash stack from the ID.
    * If there's no other information than the crash ID, ask for more details
      and add the Needs-Feedback label.

* If network causes are possible, ask for a net-internals log (If it's not a
  browser crash) and attach the most specific internals-network label that's
  applicable.  If there isn't an applicable narrower label, a clear owner for
  the issue, or there are multiple possibilities, attach the internals-network
  label and proceed with further investigation.

* If non-network causes also seem possible, attach those labels as well.

## Investigating Cr-Internals-Network bugs

* It's recommended that while on triage duty, you subscribe to the
  Cr-Internals-Network label.  To do this, go to the issue tracker and click on
  [Subscriptions](https://code.google.com/p/chromium/issues/subscriptions).
  Enter "Cr-Internals-Network" and click submit.

* Look through uncomfirmed and untriaged Cr-Internals-Network bugs,
  prioritizing those updated within the last week. [Use this issue tracker
  query](https://code.google.com/p/chromium/issues/list?can=2&q=Cr%3DInternals-Network+-status%3AAssigned+-status%3AStarted+-status%3AAvailable+&sort=-modified).

* If more information is needed from the reporter, ask for it and add the
  Needs-Feedback label.  If the reporter has answered an earlier request for
  information, remove that label.

* While investigating a new issue, change the status to Untriaged.

* If a bug is a potential security issue (Allows for code execution from remote
  site, allows crossing security boundaries, unchecked array bounds, etc) mark
  it Type-Bug-Security.  If it has privacy implication (History, cookies
  discoverable by an entity that shouldn't be able to do so, incognito state
  being saved in memory or on disk beyond the lifetime of incognito tabs, etc),
  mark it Cr-Privacy.

* For bugs that already have a more specific network label, go ahead and remove
  the Cr-Internals-Network label and move on.

* Try to figure out if it's really a network bug.  See common non-network
  labels section for description of common labels needed for issues incorrectly
  tagged as Cr-Internals-Network.

* If it's not, attach appropriate labels and go no further.

* If it may be a network bug, attach additional possibly relevant labels if
  any, and continue investigating.  Once you either determine it's a
  non-network bug, or figure out accurate more specific network labels, your
  job is done, though you should still ask for a net-internals dump if it seems
  likely to be useful.

* Note that ChromeOS-specific network-related code (Captive portal detection,
  connectivity detection, login, etc) may not all have appropriate more
  specific labels, but are not in areas handled by the network stack team.
  Just make sure those have the OS-Chrome label, and any more specific labels
  if applicable, and then move on.

* Gather data and investigate.
    * Remember to add the Needs-Feedback label whenever waiting for the user to
      respond with more information, and remove it when not waiting on the
      user.
    * Try to reproduce locally.  If you can, and it's a regression, use
      src/tools/bisect-builds.py to figure out when it regressed.
    * Ask more data from the user as needed (net-internals dumps, repro case,
      crash ID from about:crashes, run tests, etc).
    * If asking for an about:net-internals dump, provide this link:
      https://sites.google.com/a/chromium.org/dev/for-testers/providing-network-details.
      Can just grab the link from about:net-internals, as needed.

* Try to figure out what's going on, and which more specific network label is
  most appropriate.

* If it's a regression, browse through the git history of relevant files to try
  and figure out when it regressed.  CC authors / primary reviewers of any
  strongly suspect CLs.

* If you are having trouble with an issue, particularly for help understanding
  net-internals logs, email the public net-dev@chromium.org list for help
  debugging.  If it's a crasher, or for some other reason discussion needs to
  be done in private, use chrome-network-debugging@google.com.  TODO(mmenke):
  Write up a net-internals tips and tricks docs.

* If it appears to be a bug in the unowned core of the network stack (i.e. no
  sublabel applies, or only the Cr-Internals-Network-HTTP sublabel applies, and
  there's no clear owner), try to figure out the exact cause.

## Monitoring UMA histograms and Chirp/Gasper alerts

Sign up to chrome-network-debugging@google.com mailing list to receive automated
e-mails about UMA alerts.  Chirp is the new alert system, sending automated
e-mails with sender address finch-chirp@google.com.  Gasper is the old alert
system, sending automated e-mails with sender address gasper-alerts@google.com.
While Chirp is of higher priority, Gasper is not deprecated yet, so both alerts
should be monitored for the time being.

For each alert that fires, determine if it's a real alert and file a bug if so.

* Don't file if the alert is coincident with a major volume change.  The volume
  at a particular date can be determined by hovering the mouse over the
  appropriate location on the alert line.

* Don't file if the alert is on a graph with very low volume (< ~200 data
  points); it's probably noise, and we probably don't care even if it isn't.

* Don't file if the graph is really noisy (but eyeball it to decide if there is
  an underlying important shift under the noise).

* Don't file if the alert is in the "Known Ignorable" list:
    * SimpleCache on Windows
    * DiskCache on Android.

For each alert, respond to chrome-network-debugging@google.com with a summary of
the action you've taken and why, including issue link if an issue was filed.

## Investigating crashers

* Only investigate crashers that are still occurring, as identified by above
  section.  If a search on go/crash indicates a crasher is no longer occurring,
  mark it as WontFix.

* On Windows, you may want to look for weird dlls associated with the crashes.
    This generally needs crashes from a fair number of different users to reach
    any conclusions.
    * To get a list of loaded modules in related crash dumps, select
      modules->3rd party in the left pane.  It can be difficult to distinguish
      between safe dlls and those likely to cause problems, but even if you're
      not that familiar with windows, some may stick out.  Anti-virus programs,
      download managers, and more gray hat badware often have meaningful dll
      names or dll paths (Generally product names or company names).  If you
      see one of these in a significant number of the crash dumps, it may well
      be the cause.
    * You can also try selecting the "has malware" option, though that's much
      less reliable than looking manually.

* See if the same users are repeatedly running into the same issue.  This can
  be accomplished by search for (Or clicking on) the client ID associated with
  a crash report, and seeing if there are multiple reports for the same crash.
  If this is the case, it may be also be malware, or an issue with an unusual
  system/chrome/network config.

* Dig through crash reports to figure out when the crash first appeared, and
  dig through revision history in related files to try and locate a suspect CL.
  TODO(mmenke):  Add more detail here.

* Load crash dumps, try to figure out a cause.  See
  http://www.chromium.org/developers/crash-reports for more information

## Dealing with old bugs

* For all network issues (Even those with owners, or a more specific labels):

    * If the issue has had the Needs-Feedback label for over a month, verify it
      is waiting on feedback from the user.  If not, remove the label.
      Otherwise, go ahead and mark the issue WontFix due to lack of response
      and suggest the user file a new bug if the issue is still present. [Use
      this issue tracker query for old Needs-Feedback
      issues](https://code.google.com/p/chromium/issues/list?can=2&q=Cr%3AInternals-Network%20Needs=Feedback+modified-before%3Atoday-30&sort=-modified).

    * If a bug is over 2 months old, and the underlying problem was never
      reproduced or really understood:
        * If it's over a year old, go ahead and mark the issue as Archived.
        * Otherwise, ask reporters if the issue is still present, and attach
          the Needs-Feedback label.

* Old unconfirmed or untriaged Cr-Internals-Network issues can be investigated
  just like newer ones.  Crashers should generally be given higher priority,
  since we can verify if they still occur, and then newer issues, as they're
  more likely to still be present, and more likely to have a still responsive
  bug reporter.
