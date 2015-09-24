# How to get an LSS change committed

## Review

You get your change reviewed, you can upload it to
http://codereview.chromium.org (Rietveld) using `git cl upload` from Chromium's
`depot-tools`.

## Testing

Unfortunately, LSS has no automated test suite.

You can test LSS by patching it into Chromium, building Chromium, and running
Chromium's tests. (See [ProjectsUsingLSS](projects_using_lss.md).)

You can compile-test LSS by running:

    gcc -Wall -Wextra -Wstrict-prototypes -c linux_syscall_support.h

## Rolling into Chromium

If you commit a change to LSS, please also commit a Chromium change to update
`lss_revision` in Chromium's DEPS file.

This ensures that the LSS change gets tested, so that people who commit later
LSS changes don't run into problems with updating `lss_revision`.
