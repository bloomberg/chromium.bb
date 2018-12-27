const fs = require("fs");

const listing = JSON.parse(fs.readFileSync("./cts/listing.json"));

(async () => {
  for (const {path, description} of listing) {
    console.log(path, description);
    if (!path.endsWith("/")) {
      console.log(require("./cts/" + path + ".js"));
    }
  }
})();
