# Commandline flags

## Applying flags

*** note
**Note:** this requires either a `userdebug` or `eng` Android build (you can
check with `adb shell getprop ro.build.type`). Flags cannot be enabled on
production builds of Android.
***

WebView reads flags from a specific file during startup. To enable flags, write
to the file with:

```sh
$ # Overwrites all flags (and prints the new flag state):
$ build/android/adb_system_webview_command_line \
    --show-composited-layer-borders \
    --log-net-log=foo.json # Supports multiple flags
$ # Simply prints the existing flag state:
$ build/android/adb_system_webview_command_line
$ # Passing empty string clears all flags:
$ build/android/adb_system_webview_command_line ""
```

Or, you can use the `adb` in your `$PATH` like so:

```sh
$ FLAG_FILE=/data/local/tmp/webview-command-line
$ adb shell "echo '_ --show-composited-layer-borders' > ${FLAG_FILE}"
$ # The first token is ignored. We use '_' as a convenient placeholder, but any
$ # token is acceptable.
```

*** note
**Note:** either set of commands will overwrite existing flags.
***

### Applying Features with flags

WebView supports the same `--enable-features=feature1,feature2` and
`--disable-features=feature3,feature4` syntax as the rest of Chromium. You can
use these like any other flag. Please consult
[`base/feature_list.h`](https://cs.chromium.org/chromium/src/base/feature_list.h)
for details.

## Interesting flags

WebView supports any flags supported in any layer we depend on (ex. content).
Some interesting flags and Features:

 * `--show-composited-layer-borders`
 * `--enable-features=NetworkService,NetworkServiceInProcess`
 * `--log-net-log=<filename.json>`

WebView also defines its own flags and Features:

 * [AwSwitches.java](https://cs.chromium.org/chromium/src/android_webview/java/src/org/chromium/android_webview/AwSwitches.java)
   (and its [native
   counterpart](https://cs.chromium.org/chromium/src/android_webview/common/aw_switches.h))
 * [AwFeatureList.java](https://cs.chromium.org/chromium/src/android_webview/java/src/org/chromium/android_webview/AwFeatureList.java)
   (and its [native
   counterpart](https://cs.chromium.org/chromium/src/android_webview/browser/aw_feature_list.h))

## Implementation

See [CommandLineUtil.java](https://cs.chromium.org/chromium/src/android_webview/java/src/org/chromium/android_webview/command_line/CommandLineUtil.java).
