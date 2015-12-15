# Chrome Network Bug Triage

The Chrome network team uses a two day bug triage rotation.  The main goals are
to identify and label new network bugs, and investigate network bugs when no
label seems suitable.

## Responsibilities

### Required:
* Identify new crashers
* Identify new network issues.
* Request data about recent Cr-Internals-Network issue.
* Investigate each recent Cr-Internals-Network issue.
* Monitor UMA histograms and Chirp/Gasper alerts.

### Best effort:
* Investigate unowned and owned-but-forgotten net/ crashers
* Investigate old bugs
* Close obsolete bugs.

All of the above is to be done on each rotation.  These responsibilities should
be tracked, and anything left undone at the end of a rotation should be handed
off to the next triager.  The downside to passing along bug investigations like
this is each new triager has to get back up to speed on bugs the previous
triager was investigating.  The upside is that triagers don't get stuck
investigating issues after their time after their rotation, and it results in a
uniform, predictable two day commitment for all triagers.

## Details

### Required:

* Identify new crashers that are potentially network related.  You should check
  the most recent canary, the previous canary (if the most recent less than a
  day old), and any of dev/beta/stable that were released in the last couple of
  days, for each platform.  File Cr-Internals-Network bugs on the tracker when
  new crashers are found.

* Identify new network bugs, both on the bug tracker and on the crash server.
  All Unconfirmed issues filed during your triage rotation should be scanned,
  and, for suspected network bugs, a network label assigned.  A triager is
  responsible for looking at bugs reported from noon PST / 3:00 pm EST of the
  last day of the previous triager's rotation until the same time on the last
  day of their rotation.

* Investigate each recent (new comment within the past week or so)
  Cr-Internals-Network issue, driving getting information from reporters as
  needed, until you can do one of the following:

    * Mark it as *WontFix* (working as intended, obsolete issue) or a
      duplicate.

    * Mark it as a feature request.

    * Remove the Cr-Internals-Network label, replacing it with at least one
      more specific network label or non-network label.  Promptly adding
      non-network labels when appropriate is important to get new bugs in front
      of someone familiar with the relevant code, and to remove them from the
      next triager's radar.  Because of the way the bug report wizard works, a
      lot of bugs incorrectly end up with the network label.

    * The issue is assigned to an appropriate owner.

    * If there is no more specific label for a bug, it should be investigated
      until we have a good understanding of the cause of the problem, and some
      idea how it should be fixed, at which point its status should be set to
      Available.  Future triagers should ignore bugs with this status, unless
      investigating stale bugs.

* Monitor UMA histograms and Chirp/Gasper alerts.

    * For each Chirp and Gasper alert that fires, the triager should determine
      if the alert is real (not due to noise), and file a bug with the
      appropriate label if so.  Note that if no label more specific than
      Cr-Internals-Network is appropriate, the responsibility remains with the
      triager to continue investigating the bug, as above.

### Best Effort (As you have time):

* Investigate unowned and owned but forgotten net/ crashers that are still
  occurring (As indicated by
  [go/chromecrash](https://goto.google.com/chromecrash)), prioritizing frequent
  and long standing crashers.

* Investigate old bugs, prioritizing the most recent.

* Close obsolete bugs.

If you've investigated an issue (in code you don't normally work on) to an
extent that you know how to fix it, and the fix is simple, feel free to take
ownership of the issue and create a patch while on triage duty, but other tasks
should take priority.

See [bug-triage-suggested-workflow.md](bug-triage-suggested-workflow.md) for
suggested workflows.

See [bug-triage-labels.md](bug-triage-labels.md) for labeling tips for network
and non-network bugs.

See [crash-course-in-net-internals.md](crash-course-in-net-internals.md) for
some help on getting started with about:net-internals debugging.