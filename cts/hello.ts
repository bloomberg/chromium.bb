import {
  punit,
} from 'framework';

console.log(punit());

function thing() {
  return new Promise(res => {
    setTimeout(res, 100);
  });
}


export default async function() {
  describe("hello", function() {
    it("a", async function() {
      console.log('a1');
      await thing();
      console.log('a2');
      await thing();
      console.log('a3');
    });
    it("b", async function() {
      console.log('b1');
      await thing();
      console.log('b2');
      await thing();
      console.log('b3');
    });
  });
}
