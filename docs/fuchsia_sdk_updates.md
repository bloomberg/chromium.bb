# Updating Chromium to a new Fuchsia SDK

1. Check the [Fuchsia-side
   job](https://luci-scheduler.appspot.com/jobs/fuchsia/sdk-x86_64-linux) for a
   recent green archive. On the "SUCCEEDED" link, copy the SHA-1 from the
   `gsutil.upload` link of the `upload fuchsia-sdk` step.
0. Put that into Chromium's src.git DEPS in the `fuchsia_sdk` step.
0. `gclient sync && ninja ...` and make sure things go OK locally.
0. Upload the roll CL, making sure to include the `fuchsia` trybot. Tag the roll
   with `Bug: 707030`.

(Old revisions of this document have the old manual steps for building a Fuchsia
SDK if for some reason you want to do that locally. They'll probably
increasingly go out of date as the steps for building the SDK changes though.)
