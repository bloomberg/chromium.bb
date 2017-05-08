# Feature Engagement Tracker

The Feature Engagement Tracker provides a client-side backend for displaying
feature enlightenment or in-product help (IPH) with a clean and easy to use API
to be consumed by the UI frontend. The backend behaves as a black box and takes
input about user behavior. Whenever the frontend gives a trigger signal that
in-product help could be displayed, the backend will provide an answer to
whether it is appropriate to show it or not.

The frontend only needs to deal with user interactions and how to display the
feature enlightenment or in-product help itself.

The backend is feature agnostic and have no special logic for any specific
features, but instead provides a generic API.

## Compiling for platforms other than Android

For now the code for the Feature Engagement Tracker is only compiled in
for Android, but only a shim layer is really dependent on Android to provide a
JNI bridge. The goal is to keep all the business logic in the cross-platform
part of the code.

For local development, it is therefore possible to compile and run tests for
the core of the tracker on other platforms. To do this, simply add the
following line to the `//components:components_unittests` target:

```python
deps += [ "//components/feature_engagement_tracker:unit_tests" ]
```

## Testing

To compile and run tests, assuming the product out directory is `out/Debug`,
use:

```bash
ninja -C out/Debug components_unittests ;
./out/Debug/components_unittests \
  --test-launcher-filter-file=components/feature_engagement_tracker/components_unittests.filter
```

When adding new test suites, also remember to add the suite to the filter file:
`//components/feature_engagement_tracker/components_unittests.filter`.

