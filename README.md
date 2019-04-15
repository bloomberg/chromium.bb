# cts-experiment

```sh
cd cts-experiment/
yarn install

yarn global add grunt-cli  # install grunt globally
grunt  # shows available grunt commands
```

After `build` and `serve`, open:
* http://localhost:8080/?suite=out/cts/listing.json (default)
* http://localhost:8080/?suite=out/demos/listing.json
* http://localhost:8080/?suite=out/unittests/listing.json
* http://localhost:8080/?suite=out/unittests/listing.json&filter=/basic,/params&runnow=1

## TODO

* `--gtest_filter` equivalent
  * `yarn run unittests` = `yarn run cts -- --filter='unittests'`
