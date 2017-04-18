# Sensors

`device/generic_sensor` contains the platform-specific parts of the Sensor APIs
implementation.

Sensors Mojo interfaces are defined in the `public/interfaces` subdirectory.

The JS bindings are implemented in `third_party/WebKit/Source/modules/sensor`.


## Testing

Sensors platform unit tests are located in the current directory and its
subdirectories.

The sensors unit tests file for Android is
`android/junit/src/org/chromium/device/sensors/PlatformSensorAndProviderTest.java`.

Sensors browser tests are located in `content/test/data/generic_sensor`.


## Design Documents

Please refer to the [design documentation](https://docs.google.com/document/d/1Ml65ZdW5AgIsZTszk4mD_ohr40pcrdVFOIf0ZtWxDv0)
for more details.
