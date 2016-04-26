# Running Blimp

See [build](build.md) for instructions on how to build Blimp.

## Running the client

There are both Android and Linux clients.  The Android client is the shipping
client while the Linux client is used for development purposes.

### Instructing client to connect to specific host

To have the client connect to a custom engine use the `--engine-ip`,
`--engine-port`, and `--engine-transport` flags. The possible valid
values for `--engine-transport` are 'tcp' and 'ssl'.
An example valid endpoint would be
`--engine-ip=127.0.0.1 --engine-port=1234 --engine-transport=tcp`.

SSL-encrypted connections take an additional flag
`--engine-cert-path` which specifies a path to a PEM-encoded certificate
file (e.g. `--engine-cert-path=/path/to/file.pem`.)

### Requesting the complete page from the engine
The engine sends partially rendered content to the client. To request the complete
page from the engine, use the `--download-whole-document` flag on the client.

### Android Client

Install the Blimp APK with the following:

```bash
./build/android/adb_install_apk.py $(PRODUCT_DIR)/apks/Blimp.apk
```

Set up any command line flags with:

```bash
./build/android/adb_blimp_command_line --your-flag-here
```

To see your current command line, run `adb_blimp_command_line` without any
arguments.

Run the Blimp APK with:

```bash
./build/android/adb_run_blimp_client
```

### Linux Client

The blimp client is built as part of the `blimp` target. To run it with local
logging enabled, execute:

```bash
./out-linux/Debug/blimp_shell \
  --user-data-dir=/tmp/blimpclient \
  --enable-logging=stderr \
  --vmodule="*=1"
```

## Running the engine

### In a container
For running the engine in a container, see [container](container.md).

### On a workstation
To run the engine on a workstation and make your Android client connect to it,
you need to forward a port from the Android device to the host, and also
instruct the client to connect using that port on its localhost address.

#### Port forwarding
If you are running the engine on your workstation and are connected to the
client device via USB, you'll need remote port forwarding to allow the Blimp
client to talk to your computer.

##### Option A
Follow the
[remote debugging instructions](https://developer.chrome.com/devtools/docs/remote-debugging)
to get started. You'll probably want to remap 25467 to "localhost:25467".
*Note* This requires the separate `Chrome` application to be running on the
device. Otherwise you will not see the green light for the port forwarding.

##### Option B
If you are having issues with using the built-in Chrome port forwarding, you can
also start a new shell and keep the following command running:

```bash
./build/android/adb_reverse_forwarder.py --debug -v 25467 25467
```

### Required engine binary flags
* `--blimp-client-token-path=$PATH`: Path to a file containing a nonempty
  token string. If this is not present, the engine will fail to boot.
* `--use-remote-compositing`: Ensures that the renderer uses the remote
  compositor.
* `--disable-cached-picture-raster`: Ensures that rasterized content is not
  destroyed before serialization.
* `--android-fonts-path=$PATH`: Path to where the fonts are located.
  Typically this would be `out-linux/Debug/gen/third_party/blimp_fonts`.
* `--disable-remote-fonts`: Disables downloading of custom web fonts in the
  renderer.

#### Typical invocation
When the client connects to a manually specified engine instead of using the
assigner, it will use a dummy token. The engine needs to know what this token
is, so it must be provided using the `--blimp-client-token-path` flag. The token
is available in the constant `kDummyClientToken` in
`blimp/client/session/assignment_source.h`. You can easily store that to a file
by running the following command once:

```bash
awk '{if ( match($0, /^\s*const char kDummyClientToken.*/) ) { print substr($5, 2, length($5)-3);} }' \
  ./blimp/client/session/assignment_source.h > /tmp/blimpengine-token
```

Then start the engine using these flags:

```bash
out-linux/Debug/blimp_engine_app \
  --android-fonts-path=out-linux/Debug/gen/third_party/blimp_fonts \
  --blimp-client-token-path=/tmp/blimpengine-token \
  --enable-logging=stderr \
  --vmodule="blimp*=1"
```
