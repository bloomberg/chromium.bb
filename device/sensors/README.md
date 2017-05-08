# Sensors

`device/sensors` contains the platform-specific parts of the [DeviceOrientation
Events](https://www.w3.org/TR/orientation-event/) implementation.

* Sensor [Mojo](../../mojo/README.md) interfaces are defined in the
  `public/interfaces` subdirectory.
* The content renderer layer is located in `content/renderer/device_sensors`.
* The web exposed API is implemented in
  `third_party/WebKit/Source/modules/device_orientation`.

## Testing

* Browser tests are located in `content/browser/device_sensors`.
* Layout tests are located in
  `third_party/WebKit/LayoutTests/device_orientation`.
* Web platform tests are located in
  `third_party/WebKit/LayoutTests/external/wpt/orientation-event` and are a
  mirror of the [web-platform-tests GitHub
  repository](https://github.com/w3c/web-platform-tests).
