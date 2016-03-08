# Updating Blimp Fonts

1.  Clone the git-repositories listed in
    `//third_party/blimp_fonts/README.chromium`, and roll forward to the commit
    you want.
1.  Copy the necessary files to `//third_party/blimp_fonts`.
1.  Verify that the `LICENSE` file is still up to date and lists all relevant
    licenses and which fonts use which license.
1.  Update the `//third_party/blimp_fonts:fonts` target to include all the
    current fonts and their license files.
1.  Update the engine dependencies using
    `//blimp/tools/generate-engine-manifest.py`. This step is documented in
    `//blimp/docs/container.md`.
1.  Run the `upload_to_google_storage.py` (from depot_tools) script to upload
    the files. To do this, execute:

    ```bash
    find ./third_party/blimp_fonts \
        -regex '^.*.\(ttf\|otf\)$' -type f -print0 | \
        upload_to_google_storage.py --use_null_terminator -b chromium-fonts -
    ```

    If the set of fonts includes more than `.ttf` or `.otf` files, you must
    update the regular expression to include such fonts.
1.  Verify that `//third_party/blimp_fonts/.gitignore` lists the correct files
    to ignore.
1.  Add all the `.sha1` files to the chromium src repository, by executing the
    following command:

    ```bash
    git add ./third_party/blimp_fonts/*.sha1
    ```

1.  Commit and upload the change for review:

    ```bash
    git commit
    git cl upload
    ```
