# WebView Log Verbosifier

WebView Log Verbosifier is an empty dummy app (in fact, it cannot be launched).
However, if this app is installed, WebView will log the active field trials and
CommandLine flags, for debugging/QA purposes. A dummy app is used because it can
be installed on any device (including user builds, where field trials are still
relevant).

## Build and install

```shell
autoninja -C out/Default webview_log_verbosifier_apk
out/Default/bin/webview_log_verbosifier_apk install
```

## Searching logcat

You can `grep` the logcat like so:

```shell
adb logcat | grep -i 'Active field trial' # Field trials, one per line
adb logcat | grep -i 'WebViewCommandLine' # CommandLine switches, one per line
adb logcat | grep -iE 'Active field trial|WebViewCommandLine' # Both
```

Then just start up any WebView app.

## Uninstalling

When you're done investigating flags/field trials, you can disable the logging
by uninstalling the app:

```shell
out/Default/bin/webview_log_verbosifier_apk uninstall
```
