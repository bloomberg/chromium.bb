# Why am I seeing this message?

We're starting to collect metrics about how developers use gclient and other
tools in depot\_tools to better understand the performance and failure modes of
the tools, as well of the pain points and workflows of depot\_tools users.

We will collect metrics only if you're a Googler on the corp network: If you can
access https://cit-cli-metrics.appspot.com/should-upload, then we will collect
metrics from you.

The first ten executions of depot\_tools commands will print large warnings
counting down to zero. When the counter hits zero, metrics collection will
automatically begin, and depot\_tools will display large warning messages
informing you of it. These messages will continue until you explicitly opt in or
opt out.

You can run `gclient metrics --opt-in` or `gclient metrics --opt-out` to do so.
And you can opt-in or out at any time.

Please consider opting in. It will allow us to know what features are the most
important, what features can we deprecate, and what features should we develop
to better cover your use case.

## What metrics are you collecting?

First, some words about what data we are **NOT** collecting:

- We won't record any information that identifies you personally.
- We won't record the command line flag values.
- We won't record information about the current directory or environment flags.

The metrics we're collecting are:

- A timestamp, with a week resolution.
- The age of your depot\_tools checkout, with a week resolution.
- Your version of Python (in the format major.minor.micro).
- The OS of your machine (i.e. win, linux or mac).
- The arch of your machine (e.g. x64, arm, etc).
- The command that you ran (e.g. `gclient sync`).
- The flag names (but not their values) that you passed to the command
  (e.g. `--force`, `--revision`).
- The execution time.
- The exit code.
- The project you're working on. We only record data about projects you can
  fetch using depot\_tools' fetch command (e.g. Chromium, WebRTC, V8, etc)
- The age of your project checkout, with a week resolution.
- What features are you using in your DEPS and .gclient files. For example:
  - Are you setting `use\_relative\_paths=True`?
  - Are you using `recursedeps`?

## Why am I seeing this message *again*?

We might want to collect additional metrics, and if so we will ask you for
permission again.

Opting in or out explicitly will stop the messages from being displayed.

# How can I check if metrics are being collected?

You can run `gclient metrics` and it will report if you have opted in, out, or
not chosen for metrics collection.

If you have not yet explicitly opted in or out you will see a message after
each time we collect metrics.

# How can I stop seeing this message?

You will stop seeing it once you have explicitly opted in or out of depot\_tools
metrics collection.

You can run `gclient metrics --opt-in` or `gclient metrics --opt-out` to do so.
And you can opt-in or out at any time.
