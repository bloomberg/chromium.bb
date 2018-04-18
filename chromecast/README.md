# Cast base

## cast_features

This file contains tools for checking the feature state of all of the features
which affect Cast products. Cast features build upon
[the Chrome feature system](https://chromium.googlesource.com/chromium/src/+/master/base/feature_list.h).
Some aspects of Cast require the feature system to work differently, however,
so some additional logic has been layered on top. Details are available in
comments of the header file. The basics are:

 * If you are adding a new feature, add it to `cast_features.cc` so it lives
 alongside existing features
 * Add registration of your new feature via `RegisterFeature(&feature)`.
 You can add it to `RegisterFeatures()` so it lives with existing features

```c++
const base::Feature kMyFeature{"my_feature", base::FEATURE_DISABLED_BY_DEFAULT};

void RegisterFeatures() {
 // ...other features
 RegisterFeature(&kMyFeature);
}
```

 * If you are writing a unit test that touches code that reads your feature be
 sure to call `RegisterFeaturesForTesting()` in your unit test constructor
