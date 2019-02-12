# Using the trybots

[TOC]

## Overview

The trybots let committers try uncommitted patches on multiple platforms in
an automated way.

 - Trybots include all platforms for which we currently build Chromium, though
   they may not support all configurations built on CI.
 - The commit queue runs a subset of available trybots. See [here][1] for more
   information.
 - trybots can be manually invoked via `git cl try` or the "Choose Trybots"
   UI in gerrit.
 - Any committer can use the trybots.
 - Non-committers with tryjob access can also use the trybots. See [here][2]
   for more information.
 - External contributors without tryjob access can ask committers to run
   tryjobs for them.

## Workflow

1. Upload your change to gerrit via `git cl upload`
2. Run trybots:

    * Run the default set of trybots by starting a CQ dry run, either by
      setting CQ+1 on gerrit or by running `git cl try` with no arguments.
    * Run trybots of your choice by providing arguments to `git cl try`:

        * specify bucket name with `-B/--bucket`. For chromium tryjobs, this
          should always be `luci.chromium.try`
        * specify bot names with `-b/--bot`. This can be specified more than once.

### Examples

Launching a CQ dry run:

```bash
$ git cl try
```

Launching a particular trybot:

```bash
$ git cl try -B luci.chromium.try -b linux-rel
```

Launching multiple trybots:

```bash
$ git cl try -B luci.chromium.try \
  -b android-binary-size \
  -b ios-simulator-full-configs \
  -b linux-blink-rel \
  -b win7-blink-rel
  # etc
```

## Bugs? Feature requests? Questions?

[File a trooper bug.][3]

## Legacy documentation

 - [Design doc][4]

[1]: /docs/infra/cq.md
[2]: https://www.chromium.org/getting-involved/become-a-committer#TOC-Try-job-access
[3]: https://g.co/bugatrooper
[4]: https://www.chromium.org/developers/testing/try-server-usage/design
