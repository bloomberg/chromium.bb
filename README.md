# cts-experiment

```sh
cd cts-experiment/
npm install

npm install -g grunt-cli
grunt  # shows available grunt commands
```

After `build` and `serve`, open:
* http://localhost:8080/?suite=out/cts/listing.json (default)
* http://localhost:8080/?suite=out/demos/listing.json
* http://localhost:8080/?suite=out/unittests/listing.json
* http://localhost:8080/?suite=out/unittests/listing.json&filter=/basic,/params&runnow=1

## TODO

* `--gtest_filter` equivalent
  * `npm run unittests` = `npm run cts -- --filter='unittests'`
