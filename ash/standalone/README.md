Ash standalone is a version of ash intended for use without Chrome for
testing/debugging purposes. Ash typically expects Chrome to connect to it 
and configure various state (such as login). Ash standalone configures
ash in a test environment so that it does not need Chrome to connect
to it. Run it via `mash_session`, e.g.:

    out_dir/mash --service=mash_session --window-manager=ash_standalone
